# Junk to Gold

Automatically sell gray and optionally white items when players loot them. Includes configurable smart auto-sell, human player detection, and blacklist support.

## Features

- **Gray item auto-sell**: Automatically vendored on loot
- **Smart white item selling**: Optional DPS-aware comparison for weapons and quality comparison for other items
- **Human player detection**: Configure selling behavior for human-controlled vs bot players
- **Item blacklist**: Prevent specific items from being sold (e.g., quest items, shoulders)
- **Backwards compatible**: Defaults maintain original behavior (gray items only)

## Configuration

Create or edit `conf/mod_junk_to_gold.conf` in your WorldServer config directory:

```ini
[worldserver]
# Enable/disable the module
# 0 = disabled, 1 = enabled (default: 1)
JunkToGold.Enable = 1

# Print startup announcement
# 0 = disabled, 1 = enabled (default: 1)
JunkToGold.Announce = 1

# Sell white-quality items if strictly worse than equipped
# 0 = disabled (default), 1 = enabled
JunkToGold.SellCommonIfWorse = 0

# Sell white-quality weapons if strictly worse than equipped (DPS-aware)
# Warning: keeps weapon if any slot is empty or has equivalent item
# 0 = disabled (default), 1 = enabled
JunkToGold.SellWeaponsIfWorse = 0

# Enable selling for human-controlled players
# 1 = sell for humans and bots (default), 0 = sell for bots only
JunkToGold.EnableForHumans = 1

# Blacklist: comma-separated list of item IDs to never sell (no spaces)
# Example: quest items, shoulders, valuable grays
JunkToGold.Blacklist = 1793,1769,1744,1752,1777,1801,1809,1760,1785,6196
```
