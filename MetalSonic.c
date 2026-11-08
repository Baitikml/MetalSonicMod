// =====================================================================
// Metal Sonic - 6th playable character for Sonic Mania Plus (RSDKv5 decomp mod)
//
//  * Shows up as a 6th choice on the Mania Mode / No Save character card
//    (cycle: Sonic&Tails > Sonic > Tails > Knuckles > Mighty > Ray > METAL SONIC)
//  * Flies like Tails but never gets tired
//  * Never drowns (breathes underwater)
//  * Has the Sonic CD peel-out (hold UP, press JUMP)
//
// Under the hood Metal Sonic is "Sonic" (so every cutscene, boss and save file
// keeps working); the mod swaps his sprites and abilities while the Metal Sonic
// card was picked. Not available in Encore / Time Attack / Competition.
// =====================================================================
#include "Game.h"

// -------------------------
// ENGINE LINK (same as the game's own Game.c, minus the object list)
// -------------------------
RSDKFunctionTable RSDK;
APIFunctionTable API;
ModFunctionTable Mod;

DLLExport ModVersionInfo modInfo = { RETRO_REVISION, GAME_VERSION, RETRO_MOD_LOADER_VER };
const char *modID = "MetalSonic";

int32 RSDKRevision = RETRO_REVISION;

RSDKSceneInfo *SceneInfo        = NULL;
RSDKGameInfo *GameInfo          = NULL;
RSDKSKUInfo *SKU                = NULL;
RSDKControllerState *ControllerInfo = NULL;
RSDKAnalogState *AnalogStickInfoL   = NULL;
RSDKAnalogState *AnalogStickInfoR   = NULL;
RSDKTriggerState *TriggerInfoL      = NULL;
RSDKTriggerState *TriggerInfoR      = NULL;
RSDKTouchInfo *TouchInfo            = NULL;
RSDKUnknownInfo *UnknownInfo        = NULL;
RSDKScreenInfo *ScreenInfo          = NULL;

GlobalVariables *globals;

ObjectPlayer *Player;
ObjectUISaveSlot *UISaveSlot;

// -------------------------
// SETTINGS / STATE
// -------------------------
#define NOSAVE_INDEX 8 // bit 8 of metalSlots = the "No Save" slot
#define LAST_FRAME   5 // Ray is the last stock card (frameID 5)

static int32 metalSlots = 0;    // bitmask: which save slots were started as Metal Sonic (persisted)
static bool32 metalActive   = false; // current stage is being played as Metal Sonic
static uint16 metalFrames   = (uint16)-1; // Players/MetalSonic.bin
static uint16 metalUIFrames = (uint16)-1; // UI/MetalSaveSelect.bin

// per-card tracking while the menu is open
static int32 prevFrame[9];
static bool32 cardMetal[9];

// base game functions, looked up through the mod API
static void (*fn_JumpAbility_Sonic)(void);
static void (*fn_HandleAirFriction)(void);
static void (*fn_HandleAirMovement)(void);
static void (*fn_State_Ground)(void);
static void (*fn_Action_Peelout)(void);
static bool32 (*fn_TryTransform)(EntityPlayer *player, uint8 emeraldMasks);
static SaveRAM *(*fn_GetSaveRAM)(void);

// -------------------------
// METAL SONIC GAMEPLAY
// -------------------------
// Flight: Tails' flight with no tiring and no sidekick-carry logic
void MetalSonic_State_Flight(void)
{
    RSDK_THIS(Player);

    fn_HandleAirFriction();

    if (self->onGround) {
        self->state = fn_State_Ground;
        fn_HandleAirMovement();
    }
    else {
        if (self->velocity.y < -0x10000)
            self->abilitySpeed = 0x800;

        if (self->velocity.y <= 0) {
            if (self->abilityValue >= 60)
                self->abilitySpeed = 0x800;
            else
                self->abilityValue++;
        }

        self->velocity.y += self->abilitySpeed;

        // don't leave the top of the level
        if (self->camera && self->position.y < TO_FIXED(self->camera->boundsT) + 0x100000 && self->velocity.y < 0)
            self->velocity.y = 0;

        if (self->underwater) {
            RSDK.SetSpriteAnimation(self->aniFrames, ANI_SWIM, &self->animator, false, 0);
            self->animator.speed = self->velocity.y >= 0 ? 64 : 128;
        }
        else {
            RSDK.SetSpriteAnimation(self->aniFrames, ANI_FLY, &self->animator, false, 0);
            self->animator.speed = self->velocity.y >= 0 ? 128 : 256;
        }

        // keep flapping forever: never tired
        self->timer = 0;

        if (self->jumpPress) {
            self->abilitySpeed = -0x2000;
            self->abilityValue = 0;
        }
    }
}

// Pressing jump in mid-air starts the flight (Y still transforms, like Sonic)
void MetalSonic_JumpAbility(void)
{
    RSDK_THIS(Player);

    if (self->jumpPress && self->jumpAbilityState == 1) {
        if (!self->invertGravity) {
            self->jumpAbilityState = 0;
            self->abilitySpeed     = 0x800;
            self->abilityValue     = 0;
            self->timer            = 0;

            RSDK.SetSpriteAnimation(self->aniFrames, self->underwater ? ANI_SWIM : ANI_FLY, &self->animator, false, 0);
            self->state           = MetalSonic_State_Flight;
            self->nextGroundState = StateMachine_None;
            self->nextAirState    = StateMachine_None;
        }
    }
    else if (ControllerInfo[self->controllerID].keyY.press && fn_TryTransform && fn_GetSaveRAM) {
        SaveRAM *saveRAM = fn_GetSaveRAM();
        fn_TryTransform(self, saveRAM ? saveRAM->collectedEmeralds : 0);
    }
}

static void MetalSonic_ApplyToPlayer(EntityPlayer *player)
{
    // sprites (the game re-assigns Sonic's frames after respawns / super form, so check every frame)
    if (metalFrames != (uint16)-1 && player->aniFrames == Player->sonicFrames)
        player->aniFrames = metalFrames;

    // abilities
    if (player->stateAbility == fn_JumpAbility_Sonic)
        player->stateAbility = MetalSonic_JumpAbility;

    if (!player->statePeelout)
        player->statePeelout = fn_Action_Peelout;

    // breathe underwater: the drown countdown never advances
    player->drownTimer = 0;
}

static void MetalSonic_OnUpdate(void *data)
{
    if (!metalActive || !Player)
        return;

    EntityPlayer *player = RSDK_GET_ENTITY(SLOT_PLAYER1, Player);
    if (player->classID != Player->classID || player->characterID != ID_SONIC || player->sidekick)
        return;

    MetalSonic_ApplyToPlayer(player);
}

static void MetalSonic_OnStageLoad(void *data)
{
    metalActive = false;
    metalFrames = (uint16)-1;
    metalUIFrames = (uint16)-1;

    for (int32 i = 0; i < 9; ++i) {
        prevFrame[i]  = -1;
        cardMetal[i]  = false;
    }

    if (RSDK.FindObject("UISaveSlot"))
        metalUIFrames = RSDK.LoadSpriteAnimation("UI/MetalSaveSelect.bin", SCOPE_STAGE);

    if (!RSDK.FindObject("Player"))
        return;

    if (globals->gameMode == MODE_MANIA && GET_CHARACTER_ID(1) == ID_SONIC) {
        int32 index = globals->saveSlotID == NO_SAVE_SLOT ? NOSAVE_INDEX : globals->saveSlotID;
        if (index >= 0 && index <= NOSAVE_INDEX && ((metalSlots >> index) & 1)) {
            metalFrames = RSDK.LoadSpriteAnimation("Players/MetalSonic.bin", SCOPE_STAGE);
            metalActive = metalFrames != (uint16)-1;
        }
    }
}

// -------------------------
// 6TH CARD ON THE SAVE SLOT SCREEN
// -------------------------
static int32 MetalSonic_CardIndex(EntityUISaveSlot *slot)
{
    if (slot->type == UISAVESLOT_NOSAVE)
        return NOSAVE_INDEX;
    if (slot->encoreMode || slot->slotID < 0 || slot->slotID >= NOSAVE_INDEX)
        return -1; // Encore saves keep the stock roster
    return slot->slotID;
}

static void MetalSonic_OnLateUpdate(void *data)
{
    if (!UISaveSlot || !RSDK.FindObject("UISaveSlot"))
        return;

    int32 oldSlots = metalSlots;

    foreach_all(UISaveSlot, slot)
    {
        int32 i = MetalSonic_CardIndex(slot);
        if (i < 0)
            continue;

        int32 frame = slot->frameID;

        if (prevFrame[i] < 0) {
            // first time we see this card: saved games remember who they were started with
            bool32 selectable = slot->isNewSave || slot->type == UISAVESLOT_NOSAVE;
            cardMetal[i]      = !selectable && ((metalSlots >> i) & 1) && frame == 1;
            prevFrame[i]      = frame;
        }

        if (slot->isNewSave || slot->type == UISAVESLOT_NOSAVE) {
            if (frame != prevFrame[i]) {
                if (cardMetal[i]) {
                    if (prevFrame[i] == 1 && frame == 2) { // "next" from Metal Sonic -> Sonic & Tails
                        slot->frameID = 0;
                        cardMetal[i]  = false;
                    }
                    else if (prevFrame[i] == 1 && frame == 0) { // "previous" from Metal Sonic -> Ray
                        slot->frameID = LAST_FRAME;
                        cardMetal[i]  = false;
                    }
                    else {
                        cardMetal[i] = false;
                    }
                }
                else {
                    if ((prevFrame[i] == LAST_FRAME && frame == 0) || (prevFrame[i] == 0 && frame == LAST_FRAME)) {
                        slot->frameID = 1; // shown with Sonic's slot data, flagged as Metal Sonic
                        cardMetal[i]  = true;
                    }
                }
                prevFrame[i] = slot->frameID;
            }

            if (cardMetal[i] && slot->frameID != 1)
                cardMetal[i] = false;

            if (cardMetal[i])
                metalSlots |= 1 << i;
            else
                metalSlots &= ~(1 << i);
        }
        else if (slot->frameID != 1) {
            cardMetal[i] = false;
        }
    }

    if (metalSlots != oldSlots) {
        Mod.SetSettingsInteger("Config:metalSlots", metalSlots);
        Mod.SaveSettings();
    }
}

// draw the card with Metal Sonic's portrait / life icon instead of Sonic's
void MetalSonic_UISaveSlot_Draw(void)
{
    RSDK_THIS(UISaveSlot);

    int32 i             = MetalSonic_CardIndex(self);
    uint16 originalAnim = UISaveSlot->aniFrames;
    bool32 swap         = i >= 0 && cardMetal[i] && metalUIFrames != (uint16)-1 && self->frameID == 1;

    if (swap)
        UISaveSlot->aniFrames = metalUIFrames;

    Mod.Super(UISaveSlot->classID, SUPER_DRAW, NULL);

    UISaveSlot->aniFrames = originalAnim;
}

// -------------------------
// MOD ENTRY POINT
// -------------------------
void InitModAPI(void)
{
    fn_JumpAbility_Sonic = Mod.GetPublicFunction(NULL, "Player_JumpAbility_Sonic");
    fn_HandleAirFriction = Mod.GetPublicFunction(NULL, "Player_HandleAirFriction");
    fn_HandleAirMovement = Mod.GetPublicFunction(NULL, "Player_HandleAirMovement");
    fn_State_Ground      = Mod.GetPublicFunction(NULL, "Player_State_Ground");
    fn_Action_Peelout    = Mod.GetPublicFunction(NULL, "Player_Action_Peelout");
    fn_TryTransform      = Mod.GetPublicFunction(NULL, "Player_TryTransform");
    fn_GetSaveRAM        = Mod.GetPublicFunction(NULL, "SaveGame_GetSaveRAM");

    metalSlots = Mod.GetSettingsInteger("", "Config:metalSlots", 0);
    Mod.SetSettingsInteger("Config:metalSlots", metalSlots);

    MOD_REGISTER_OBJECT_HOOK(Player);

    // overload only the draw of UISaveSlot (also links the static variables)
    Mod.RegisterObject((void **)&UISaveSlot, NULL, "UISaveSlot", sizeof(EntityUISaveSlot), sizeof(ObjectUISaveSlot), 0, NULL, NULL, NULL,
                       MetalSonic_UISaveSlot_Draw, NULL, NULL, NULL, NULL, NULL, NULL, "UISaveSlot");

    Mod.AddModCallback(MODCB_ONSTAGELOAD, MetalSonic_OnStageLoad);
    Mod.AddModCallback(MODCB_ONUPDATE, MetalSonic_OnUpdate);
    Mod.AddModCallback(MODCB_ONLATEUPDATE, MetalSonic_OnLateUpdate);
}

void LinkGameLogicDLL(EngineInfo *info)
{
    memset(&API, 0, sizeof(APIFunctionTable));
    memset(&RSDK, 0, sizeof(RSDKFunctionTable));

    if (info->functionTable)
        memcpy(&RSDK, info->functionTable, sizeof(RSDKFunctionTable));

    if (info->APITable)
        memcpy(&API, info->APITable, sizeof(APIFunctionTable));

    if (info->modTable)
        memcpy(&Mod, info->modTable, sizeof(ModFunctionTable));

    GameInfo       = info->gameInfo;
    SKU            = info->currentSKU;
    SceneInfo      = info->sceneInfo;
    ControllerInfo = info->controllerInfo;

    AnalogStickInfoL = info->stickInfoL;
    AnalogStickInfoR = info->stickInfoR;
    TriggerInfoL     = info->triggerInfoL;
    TriggerInfoR     = info->triggerInfoR;
    TouchInfo        = info->touchInfo;

    UnknownInfo = info->unknownInfo;
    ScreenInfo  = info->screenInfo;
}

DLLExport bool32 LinkModLogic(EngineInfo *info, const char *id)
{
    LinkGameLogicDLL(info);

    globals = Mod.GetGlobals();
    modID   = id;

    InitModAPI();

    return true;
}
