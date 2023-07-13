#include "common.h"

// ----------------------------------------------------------------
// Requirements

// actor part of the first link in the chain
#ifndef CHOMP_CHAIN_FIRST_PART_IDX
    #error CHOMP_CHAIN_FIRST_PART_IDX is not defined!
#endif

// actor part of the last link in the chain
#ifndef CHOMP_CHAIN_LAST_PART_IDX
    #error CHOMP_CHAIN_LAST_PART_IDX  is not defined!
#endif

// actor var index which determines if chain sounds can play
#ifndef CHOMP_CHAIN_AVAR_SOUNDS
    #error CHOMP_CHAIN_AVAR_SOUNDS  is not defined!
#endif

// ----------------------------------------------------------------
// Options

// define this TRUE to enable updating the z-pos of the chain
// CHOMP_CHAIN_UPDATE_Z

// ----------------------------------------------------------------

#define NUM_CHAIN_LINKS (1 + CHOMP_CHAIN_LAST_PART_IDX - CHOMP_CHAIN_FIRST_PART_IDX)

API_CALLABLE(N(ChompChainInit)) {
    Actor* actor = get_actor(script->owner1.actorID);
    ActorPart* actorPart;
    ChompChain* chainParts;
    s32 baseChainPart = CHOMP_CHAIN_FIRST_PART_IDX;
    f32 x, y, z;
    s32 i;

    chainParts = heap_malloc(NUM_CHAIN_LINKS * sizeof(*chainParts));
    actor->state.functionTempPtr[0] = chainParts;

    x = actor->currentPos.x + 12.0;
    y = actor->currentPos.y + 5.0;
    z = actor->currentPos.z;

    for (i = 0; i < NUM_CHAIN_LINKS; i++, chainParts++) {
        chainParts->outerLinkLen = 7.0f;
        chainParts->linkLengthZ = 7.0f;
        chainParts->innerLinkLen = 7.0f;
        chainParts->currentPos.x = x;
        chainParts->currentPos.y = y;
        chainParts->currentPos.z = z;
        chainParts->settleAmt = 0;
        chainParts->settleRate = 0.6f;
        chainParts->gravAccel = 3.0f;
        chainParts->velY = 0;
        actorPart = get_actor_part(actor, baseChainPart + i);
        actorPart->absolutePosition.x = chainParts->currentPos.x;
        actorPart->absolutePosition.y = chainParts->currentPos.y;
        actorPart->absolutePosition.z = chainParts->currentPos.z;
    }
    return ApiStatus_DONE2;
}

void N(ChompChainAddPolarPos)(ChompChain* script, f32 magnitude, f32 angleDeg) {
    f32 angle = DEG_TO_RAD(angleDeg);
    f32 dirX = sin_rad(angle);
    f32 dirY = cos_rad(angle);

    script->currentPos.x += -magnitude * dirX;
    script->currentPos.y += magnitude * dirY;
}

void N(ChompChainGetPolarX)(f32* x, f32 magnitude, f32 angleDeg) {
    f32 angle = DEG_TO_RAD(angleDeg);
    f32 dirX = sin_rad(angle);
    f32 dirY = cos_rad(angle);

    *x = magnitude * dirY;
}

API_CALLABLE(N(ChompChainUpdate)) {
    Bytecode* args = script->ptrReadPos;
    f32 sp18;
    Actor* actor;
    ActorPart* part;
    ChompChain* chain;
    f32 dist;
    f32 angle;
    f32 prevX, prevY;
    s32 baseChainPart;
    s32 posZ;
    s32 i;

    actor = get_actor(script->owner1.actorID);
    if (actor == NULL) {
        return ApiStatus_BLOCK;
    }

    #if CHOMP_CHAIN_UPDATE_Z == TRUE
    posZ = evt_get_variable(script, *args++);
    #endif

    baseChainPart = CHOMP_CHAIN_FIRST_PART_IDX;

    // initialize prev positions to the rear of the actor's body
    chain = actor->state.functionTempPtr[0];
    if (actor->debuff == STATUS_KEY_SHRINK) {
        prevX = actor->currentPos.x + 6.0;
        prevY = actor->currentPos.y + 2.5;
    } else {
        prevX = actor->currentPos.x + 12.0;
        prevY = actor->currentPos.y + 5.0;
    }

    // update each link in the chain
    for (i = 0; i < NUM_CHAIN_LINKS; i++, chain++) {
        if (actor->debuff == STATUS_KEY_SHRINK) {
            chain->outerLinkLen = 3.5f;
            chain->linkLengthZ = 3.5f;
            chain->innerLinkLen = 3.5f;
        } else {
            chain->outerLinkLen = 7.0f;
            chain->linkLengthZ = 7.0f;
            chain->innerLinkLen = 7.0f;
        }

        // add gravity and clamp velocity at -2G
        chain->velY -= chain->gravAccel;
        if (chain->velY < 2.0f * -chain->gravAccel) {
            chain->velY = 2.0f * -chain->gravAccel;
            if (actor->state.varTable[CHOMP_CHAIN_AVAR_SOUNDS] && i == 0) {
                sfx_play_sound_at_position(SOUND_2063, SOUND_SPACE_MODE_0, actor->currentPos.x, actor->currentPos.y, actor->currentPos.z);
            }
        }

        // add velocity and clamp position to roughly the radius of the chain (assuming ground at y = 0)
        chain->currentPos.y += chain->velY;
        if (actor->debuff == STATUS_KEY_SHRINK) {
            if (chain->currentPos.y < 2.5) {
                chain->currentPos.y = 2.5f;
                chain->velY = 0.0f;
            }
        } else {
            if (chain->currentPos.y < 5.0) {
                chain->currentPos.y = 5.0f;
                chain->velY = 0.0f;
            }
        }

        // get distance from previous part of the chain
        dist = dist2D(prevX, prevY, chain->currentPos.x, chain->currentPos.y);
        angle = atan2(prevX, prevY, chain->currentPos.x, chain->currentPos.y);

        if (dist >= chain->linkLengthZ) {
            N(ChompChainGetPolarX)(&sp18, dist - chain->linkLengthZ, angle);
            chain->velY += sp18 * 0.5;
        }

        if (dist >= chain->innerLinkLen) {
            f32 moveLen;

            if (dist >= chain->outerLinkLen) {
                // clamp position to be outerLinkLen away from previous part
                moveLen = dist;
                moveLen = dist - chain->outerLinkLen;
            } else {
                chain->settleAmt += chain->settleRate;
                moveLen = chain->settleAmt;
            }
            N(ChompChainAddPolarPos)(chain, moveLen, angle);
        } else {
            chain->settleAmt -= chain->settleRate * 0.2;
            if (chain->settleAmt < 0.0) {
                chain->settleAmt = 0.0f;
            }
            N(ChompChainAddPolarPos)(chain, chain->settleAmt, angle);
        }
        if (chain->settleAmt > 4.0) {
            chain->settleAmt = 4.0f;
        }
        
        #if CHOMP_CHAIN_UPDATE_Z == TRUE
        chain->currentPos.z = posZ;
        #endif

        part = get_actor_part(actor, baseChainPart + i);
        part->absolutePosition.x = chain->currentPos.x;
        part->absolutePosition.y = chain->currentPos.y;
        part->absolutePosition.z = chain->currentPos.z;

        if (actor->debuff == STATUS_KEY_SHRINK) {
            part->scale.x = 0.5f;
            part->scale.y = 0.5f;
            part->scale.z = 1.0f;
        } else {
            part->scale.x = 1.0f;
            part->scale.y = 1.0f;
            part->scale.z = 1.0f;
        }
        prevX = chain->currentPos.x;
        prevY = chain->currentPos.y;
    }

    return ApiStatus_DONE2;
}
