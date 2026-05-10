#include "j2g_conf.h"
#include "Config.h"
#include "Log.h"
#include <unordered_set>

namespace
{
    bool sEnabled  = true;
    bool sAnnounce = true;
	bool sEnableForHumans = true;
	bool sSellCommonIfWorse = false;
	bool sSellWeaponsIfWorse = false;
    std::unordered_set<uint32> sBlacklist;
}

namespace J2G
{
    bool IsEnabled()
    {
        return sEnabled;
    }

    void LoadConfig()
    {
        sEnabled  = sConfigMgr->GetOption<bool>("JunkToGold.Enable",   true);
        sAnnounce = sConfigMgr->GetOption<bool>("JunkToGold.Announce", true);
		sEnableForHumans = sConfigMgr->GetOption<bool>("JunkToGold.EnableForHumans", true);
        sSellCommonIfWorse = sConfigMgr->GetOption<bool>("JunkToGold.SellCommonIfWorse", false);
        sSellWeaponsIfWorse = sConfigMgr->GetOption<bool>("JunkToGold.SellWeaponsIfWorse", false);

        // Parse blacklist
        sBlacklist.clear();
        std::string blacklistStr = sConfigMgr->GetOption<std::string>("JunkToGold.Blacklist", "");
        if (!blacklistStr.empty())
        {
            size_t pos = 0;
            while (pos < blacklistStr.length())
            {
                size_t nextPos = blacklistStr.find(',', pos);
                if (nextPos == std::string::npos)
                    nextPos = blacklistStr.length();

                std::string itemIdStr = blacklistStr.substr(pos, nextPos - pos);
                if (!itemIdStr.empty())
                {
                    try
                    {
                        uint32 itemId = std::stoul(itemIdStr);
                        sBlacklist.insert(itemId);
                    }
                    catch (const std::exception&)
                    {
                        // Skip invalid entries
                    }
                }
                pos = nextPos + 1;
            }
        }

        if (sAnnounce)
        {
            LOG_INFO("module", "mod-junk-to-gold: {}", sEnabled ? "enabled" : "disabled");
            LOG_INFO("module", "mod-junk-to-gold: JunkToGold.Announce {}", sAnnounce ? "enabled" : "disabled");
            LOG_INFO("module", "mod-junk-to-gold: JunkToGold.EnableForHumans {}", sEnableForHumans ? "enabled" : "disabled");
            LOG_INFO("module", "mod-junk-to-gold: JunkToGold.SellCommonIfWorse {}", sSellCommonIfWorse ? "enabled" : "disabled");
            LOG_INFO("module", "mod-junk-to-gold: JunkToGold.SellWeaponsIfWorse {}", sSellWeaponsIfWorse ? "enabled" : "disabled");
            LOG_INFO("module", "mod-junk-to-gold: JunkToGold.Blacklist contains {} items", sBlacklist.size());
        }
    }

    bool EnableForHumans()
    {
        return sEnableForHumans;
    }
	 
    bool SellCommonIfWorse()
    {
        return sSellCommonIfWorse;
    }

    bool SellWeaponsIfWorse()
    {
        return sSellWeaponsIfWorse;
    }

    bool IsBlacklisted(uint32 itemId)
    {
        return sBlacklist.find(itemId) != sBlacklist.end();
    }

    void World::OnAfterConfigLoad(bool /*reload*/)
    {
        LoadConfig();
    }
}
