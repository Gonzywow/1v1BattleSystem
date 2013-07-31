/*
 *╔═╦═╦═╦╦╦══╦═╦╗─╔╦══╗ 
 *║╦╣║║║║║╠╗╗║╦╣╚╦╝║══╣
 *║╩╣║║║║║╠╩╝║╩╬╗║╔╬══║
 *╚═╩╩═╩╩═╩══╩═╝╚═╝╚══╝
 *       EmuDevs - (http://emudevs.com)
*/
#ifndef _HAMARBATTLE_H
#define _HAMARBATTLE_H

#include "Map.h"
#include "MapManager.h"

struct QueueInfo
{
    uint64 opponentGuid;
    bool acceptedBattle;
    bool requestedToBattle;
    uint32 acceptedTimer;
};

struct PlayerPositions
{
    uint32 mapId;
    float x, y, z, o;
};

enum HamarBattleStatus
{
    HAMAR_WAIT        = 0,
    HAMAR_IN_PROGRESS = 1,
    HAMAR_ENDED       = 2
};

enum HamarStartEvents
{
    EVENT_STARTING_ANNOUNCE_15S = 1,
    EVENT_ANNOUNCE_EVERY_SECOND = 2,
    EVENT_START_DESPAWN         = 3,
    EVENT_DESPAWN_DOORS         = 4,
    EVENT_COMPLETED             = 5
};

enum HamarObjectId
{
    BG_HAM_OBJECTID_GATE               = 180424,
};

const float Door_positions[2][8] =
{   // x            y       z          o       0        1       2      3
    {2172.56f, -4788.08f, 55.138f, 1.354140f, 0, 0, 0.626512f, 0.779412f},
    {2196.92f, -4750.05f, 55.138f, 3.792810f, 0, 0, 0.947457f, -0.319884f}
};

class HamarQueueu
{
public:
    friend class ACE_Singleton<HamarQueueu, ACE_Null_Mutex>;
    // Player checks
    bool isInQueue(uint64 guid);

    void AddPlayer(uint64 guid);
    void RemovePlayer(uint64 guid);
    void Update(uint32 diff);

    void RemoveOfflinePlayers();
    void ProcessQueuePlayers();
    void ProcessAcceptedQueuePlayers(uint32 diff);

    std::map<uint64, QueueInfo*> GetQueuers(bool normal);

    QueueInfo* GetQueueInfo(uint64 guid);

    bool ReceiveBattleAnswer(Player* player, uint32 sender, uint32 action, uint32 menuId);
    void SendBattleRequest(uint64 guid);
private:
    std::map<uint64, QueueInfo*> queueList;
    uint32 updateTimer;
    // their guid and bool accepted, opponent storing?
};
#define sHamarQueueMgr ACE_Singleton<HamarQueueu, ACE_Null_Mutex>::instance()

class HamarBattle
{
public:
    HamarBattle();
    explicit HamarBattle(Player* plr, Player* plr2);
    ~HamarBattle();

    void Update(uint32 diff);
    inline void ProcessWait(uint32 diff);
    void HandlePlayerKill(Player * victim, Player * killer);
    void TeleportPlayers();
    void RemovePlayers();
    bool SetupBattleGround();
    void EndBattleGround(Player* winner);
    bool DeleteObjects();
    bool PlayerOnline(uint64 guid);
    void StorePlayerLocations();
    bool CanUpdate();
    void AnnounceToPlayers(std::string text);
    bool AddObject(uint32 entry, float x, float y, float z, float o, float rotation0, float rotation1, float rotation2, float rotation3, uint32 respawnTime = 0);
    Map* GetHamarMap() { return map; }

    typedef std::list<uint64> BGobjects;
    BGobjects objects;

private:
    Player* player1;
    Player* player2;
    PlayerPositions plr1Pos;
    PlayerPositions plr2Pos;
    uint32 m_battleTimer;
    uint32 m_waitTimer;
    uint32 m_checkupTimer;
    uint32 map_id;
    uint32 phaseMask;
    Map* map;
    HamarBattleStatus m_status;
    HamarStartEvents m_startEvent;
    uint64 plrOneGuid;
    uint64 plrTwoGuid;
};

class HamarBattleManager
{
public:
    friend class ACE_Singleton<HamarBattleManager, ACE_Null_Mutex>;
    void Update(uint32 diff);
    bool CreateBattle(uint64 playerGuid, uint64 player2Guid);
    void RemoveBattle(HamarBattle *hm);
    void CreatePhases();
    uint32 CreatePhaseMask();
    bool IsFreeSlots();
    void ReturnPhaseMask(uint32 phaseMask);
private:
    std::list<HamarBattle*> m_battles;
    std::map<uint32, bool> m_availablePhases;
    uint32 UpdateTimer;
};

#define sHamarMgr ACE_Singleton<HamarBattleManager, ACE_Null_Mutex>::instance()
#endif // !#ifndef _HAMARBATTLE_H