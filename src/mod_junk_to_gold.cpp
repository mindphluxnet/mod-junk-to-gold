#include "Chat.h"
#include "Item.h"
#include "j2g_conf.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
//#include "Log.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Detect module playerbots
#if defined(__has_include)
#  if __has_include("Script/Playerbots.h")
#    include "Script/Playerbots.h"
#    define J2G_HAVE_PLAYERBOTS 1
#  elif __has_include("Bot/PlayerbotMgr.h") && __has_include("Bot/PlayerbotAI.h")
#    include "Bot/PlayerbotMgr.h"
#    include "Bot/PlayerbotAI.h"
#    define J2G_HAVE_PLAYERBOTS 1
#  endif
#endif

static inline bool J2G_IsBot(Player* p)
{
#ifdef J2G_HAVE_PLAYERBOTS
    return p && sPlayerbotsMgr.GetPlayerbotAI(p);
#else
    return false;
#endif
}

static inline WorldSession* J2G_GetMasterSession(Player* player)
{
    if (!player)
        return nullptr;

#ifdef J2G_HAVE_PLAYERBOTS
    if (PlayerbotAI* ai = sPlayerbotsMgr.GetPlayerbotAI(player))
        if (Player* master = ai->GetMaster())
            if (WorldSession* masterSession = master->GetSession())
                return masterSession;
#endif

    return player->GetSession();
}

static inline bool IsWeaponInvType(uint32 invType)
{
    switch (invType)
    {
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_2HWEAPON:
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
#ifdef INVTYPE_WAND
        case INVTYPE_WAND:
#endif
            return true;
        default:
            return false;
    }
}

static inline double ComputeWeaponDPS(ItemTemplate const* proto)
{
    if (!proto || proto->Delay == 0)
        return 0.0;
    // Use the first valid damage line
    double minD = 0.0, maxD = 0.0;
    if (proto->Damage[0].DamageMin > 0.0 || proto->Damage[0].DamageMax > 0.0)
    {
        minD = proto->Damage[0].DamageMin;
        maxD = proto->Damage[0].DamageMax;
    }
    else if (proto->Damage[1].DamageMin > 0.0 || proto->Damage[1].DamageMax > 0.0)
    {
        minD = proto->Damage[1].DamageMin;
        maxD = proto->Damage[1].DamageMax;
    }
    double avg = (minD + maxD) * 0.5;
    return (avg * 1000.0) / double(proto->Delay); // dmg per second
}

static inline bool CanPlayerUseItemTemplate(Player* player, ItemTemplate const* proto)
{
    if (!player || !proto)
        return false;

    // Minimal "can use" check (class/skill/restrictions). Core-friendly.
    return player->CanUseItem(proto) == EQUIP_ERR_OK;
}

static inline bool IsDpsClose(double a, double b)
{
    // Aggressive: tiny tolerance, so we sell more often when DPS is even slightly lower.
    // Threshold: max(0.01 DPS, 0.2% relative)
    double absTol = 0.01;
    double relTol = 0.002 * std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= std::max(absTol, relTol);
}

static inline int CompareForEquipDecision(ItemTemplate const* a, ItemTemplate const* b, bool weaponPreferredDPS)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    if (a->Quality != b->Quality)
        return int(a->Quality) - int(b->Quality);
    if (weaponPreferredDPS)
    {
        double dpsA = ComputeWeaponDPS(a), dpsB = ComputeWeaponDPS(b);
        if (!IsDpsClose(dpsA, dpsB))
        {
            // Aggressive: if DPS is lower, treat it as worse without extra safeguards.
            return dpsA > dpsB ? 1 : -1;
        }
        // DPS close enough: fall back to ilvl/reqlevel
    }
    if (a->ItemLevel != b->ItemLevel)
        return int(a->ItemLevel) - int(b->ItemLevel);
    if (a->RequiredLevel != b->RequiredLevel)
        return int(a->RequiredLevel) - int(b->RequiredLevel);
    return 0;
}

static void GetCandidateSlots(uint32 invType, std::vector<uint8>& slots)
{
    switch (invType)
    {
        case INVTYPE_HEAD:        slots.push_back(EQUIPMENT_SLOT_HEAD); break;
        case INVTYPE_NECK:        slots.push_back(EQUIPMENT_SLOT_NECK); break;
        case INVTYPE_SHOULDERS:   slots.push_back(EQUIPMENT_SLOT_SHOULDERS); break;
        case INVTYPE_BODY:        slots.push_back(EQUIPMENT_SLOT_BODY); break;     // chemise
        case INVTYPE_CHEST:       slots.push_back(EQUIPMENT_SLOT_CHEST); break;
        case INVTYPE_ROBE:        slots.push_back(EQUIPMENT_SLOT_CHEST); break;
        case INVTYPE_WAIST:       slots.push_back(EQUIPMENT_SLOT_WAIST); break;
        case INVTYPE_LEGS:        slots.push_back(EQUIPMENT_SLOT_LEGS); break;
        case INVTYPE_FEET:        slots.push_back(EQUIPMENT_SLOT_FEET); break;
        case INVTYPE_WRISTS:      slots.push_back(EQUIPMENT_SLOT_WRISTS); break;
        case INVTYPE_HANDS:       slots.push_back(EQUIPMENT_SLOT_HANDS); break;
        case INVTYPE_FINGER:      slots.push_back(EQUIPMENT_SLOT_FINGER1); slots.push_back(EQUIPMENT_SLOT_FINGER2); break;
        case INVTYPE_TRINKET:     slots.push_back(EQUIPMENT_SLOT_TRINKET1); slots.push_back(EQUIPMENT_SLOT_TRINKET2); break;
        case INVTYPE_CLOAK:       slots.push_back(EQUIPMENT_SLOT_BACK); break;
        case INVTYPE_SHIELD:      slots.push_back(EQUIPMENT_SLOT_OFFHAND); break;
        // --- Arms ---
        case INVTYPE_WEAPON:          // can go in main hand OR off-hand
            slots.push_back(EQUIPMENT_SLOT_MAINHAND);
            slots.push_back(EQUIPMENT_SLOT_OFFHAND);
            break;
        case INVTYPE_WEAPONMAINHAND:  slots.push_back(EQUIPMENT_SLOT_MAINHAND); break;
        case INVTYPE_WEAPONOFFHAND:   slots.push_back(EQUIPMENT_SLOT_OFFHAND);  break;
        case INVTYPE_2HWEAPON:        slots.push_back(EQUIPMENT_SLOT_MAINHAND); break;
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
#ifdef INVTYPE_WAND
        case INVTYPE_WAND:
#endif
                                      slots.push_back(EQUIPMENT_SLOT_RANGED);   break;
        default: break;
    }
}

static bool IsWhiteItemWorseThanEquipped(Player* player, ItemTemplate const* proto, bool weapon)
{
    if (!player || !proto)
        return false;

    std::vector<uint8> slots;
    GetCandidateSlots(proto->InventoryType, slots);
    if (slots.empty())
        return false; // non-equippable (not relevant)

    // Aggressive: if the player cannot use the item, it can never be an upgrade -> auto-sell.
    if (!CanPlayerUseItemTemplate(player, proto))
        return true;

    // If any candidate slot is empty OR the loot is >= an equipped item -> don't sell (possible upgrade)
    bool strictlyWorse = false;
    for (uint8 slot : slots)
    {
        Item* eq = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!eq)
            return false; // empty slot -> potentially useful
        ItemTemplate const* eqProto = eq->GetTemplate();
        if (CompareForEquipDecision(proto, eqProto, weapon) >= 0)
            return false; // proto >= an equipped item -> keep it
        strictlyWorse = true; // proto < eqProto for this slot
    }
    return strictlyWorse;
}

class JunkToGold : public PlayerScript
{
public:
    JunkToGold() : PlayerScript("JunkToGold") {}

	void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid /*lootGuid*/) override
    {
        // --- Global Toggle ---
        if (!J2G::IsEnabled())
            return; // // module off: do nothing

        /*LOG_INFO("module",
                 "mod-junk-to-gold: OnPlayerLootItem player={} guid={} isBot={} itemEntry={} count={}",
                 player ? player->GetName() : "<null>",
                 player ? player->GetGUID().ToString() : "<null>",
                 J2G_IsBot(player),
                 item ? item->GetEntry() : 0,
                 count);*/

        // If selling for humans is disabled, only process bots (when playerbots are available)
        if (!J2G::EnableForHumans() && !J2G_IsBot(player))
            return; // skip humans, keep bot loot handling

        if (!item)
            return;
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return;

        bool shouldSell = false;
        // (1) Grey: always sold
        if (proto->Quality == ITEM_QUALITY_POOR)
            shouldSell = true;
        // (2) White: only (if enabled) when worse than equipped
        else if (proto->Quality == ITEM_QUALITY_NORMAL)
        {
            bool isWeapon = IsWeaponInvType(proto->InventoryType);
            if (isWeapon && J2G::SellWeaponsIfWorse())
                shouldSell = IsWhiteItemWorseThanEquipped(player, proto, /*weapon=*/true);
            else if (!isWeapon && J2G::SellCommonIfWorse())
                shouldSell = IsWhiteItemWorseThanEquipped(player, proto, /*weapon=*/false);
        }

        if (!shouldSell)
            return; // we kkep item in bag

        // Sale: info message + gold conversion + removal (once only)
        SendTransactionInformation(player, item, count);
        /*LOG_INFO("module",
                 "mod-junk-to-gold: Sold item player={} entry={} count={} sellPrice={}",
                 player ? player->GetName() : "<null>",
                 item->GetEntry(),
                 count,
                 proto->SellPrice);*/
        player->ModifyMoney(int64(uint64(proto->SellPrice) * uint64(count)));
        //player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

        // If the loot stacked into an existing slot, 'item' can represent a stack larger than 'count'.
        // DestroyItem(bag, slot) would delete the whole stack -> item loss.
        uint32 stackCount = item->GetCount();
        if (stackCount > count)
        {
            // Remove only the looted amount from the existing stack.
            item->SetCount(stackCount - count);
            item->SetState(ITEM_CHANGED, player);

            // Keep quest/item removal checks consistent with normal removals.
            player->ItemRemovedQuestCheck(item->GetEntry(), count);
        }
        else
        {
            // Full stack (or exact amount) -> remove the slot as before.
            player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        }
    }

private:
    void SendTransactionInformation(Player* player, Item* item, uint32 count)
    {
        std::string seller = player ? player->GetName() : "<unknown>";

        std::string name;
        // Color based on quality (0=grey, 1=white)
        const char* qColor = "|cff9d9d9d"; // poor
        switch (item->GetTemplate()->Quality)
        {
            case ITEM_QUALITY_NORMAL: qColor = "|cffffffff"; break; // white
            case ITEM_QUALITY_POOR:   qColor = "|cff9d9d9d"; break; // grey
            default: break;
        }
        if (count > 1)
        {
            name = Acore::StringFormat("{}|Hitem:{}::::::::80:::::|h[{}]|h|rx{}",
                                       qColor, item->GetTemplate()->ItemId, item->GetTemplate()->Name1, count);
        }
        else
        {
            name = Acore::StringFormat("{}|Hitem:{}::::::::80:::::|h[{}]|h|r",
                                       qColor, item->GetTemplate()->ItemId, item->GetTemplate()->Name1);
        }

        uint64 money = uint64(item->GetTemplate()->SellPrice) * uint64(count);
        uint64 gold = money / GOLD;
        uint64 silver = (money % GOLD) / SILVER;
        uint64 copper = (money % GOLD) % SILVER;

        std::string info;
        if (money < SILVER)
        {
            info = Acore::StringFormat("{}: {} sold for {} copper.", seller, name, copper);
        }
        else if (money < GOLD)
        {
            if (copper > 0)
            {
                info = Acore::StringFormat("{}: {} sold for {} silver and {} copper.", seller, name, silver, copper);
            }
            else
            {
                info = Acore::StringFormat("{}: {} sold for {} silver.", seller, name, silver);
            }
        }
        else
        {
            if (copper > 0 && silver > 0)
            {
                info = Acore::StringFormat("{}: {} sold for {} gold, {} silver and {} copper.", seller, name, gold, silver, copper);
            }
            else if (copper > 0)
            {
                info = Acore::StringFormat("{}: {} sold for {} gold and {} copper.", seller, name, gold, copper);
            }
            else if (silver > 0)
            {
                info = Acore::StringFormat("{}: {} sold for {} gold and {} silver.", seller, name, gold, silver);
            }
            else
            {
                info = Acore::StringFormat("{}: {} sold for {} gold.", seller, name, gold);
            }
        }

        WorldSession* session = J2G_GetMasterSession(player);
        /*LOG_INFO("module",
                 "mod-junk-to-gold: SendTransactionInformation player={} isBot={} session={} info={}",
                 player ? player->GetName() : "<null>",
                 J2G_IsBot(player),
                 session ? session->GetPlayerName() : "<null>",
                 info);*/
        ChatHandler(session).SendSysMessage(info);
    }
};

void Addmod_junk_to_goldScripts()
{
    J2G::LoadConfig();

    new J2G::World();
	
    new JunkToGold();
}
