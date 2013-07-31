/*
 *╔═╦═╦═╦╦╦══╦═╦╗─╔╦══╗ 
 *║╦╣║║║║║╠╗╗║╦╣╚╦╝║══╣
 *║╩╣║║║║║╠╩╝║╩╬╗║╔╬══║
 *╚═╩╩═╩╩═╩══╩═╝╚═╝╚══╝
 *       EmuDevs - (http://emudevs.com)
*/
#include "hamarbattle.h"
#include "Player.h"
#include "Chat.h"
#include "SpellAuras.h"
#include "GossipDef.h"
#include "ScriptedGossip.h"


#define QUEUE_UPDATE_INTERVAL 1000
// Queueu System
void HamarQueueu::AddPlayer(uint64 guid)
{
    QueueInfo* info = GetQueueInfo(guid);
    if(!info)
    {
        info = new QueueInfo;
        info->acceptedBattle = false;
        info->opponentGuid = NULL;
        info->requestedToBattle = false;
        info->acceptedTimer     = 0;

        queueList.insert(std::pair<uint64, QueueInfo*>(guid, info));
    }
}

void HamarQueueu::RemovePlayer(uint64 guid)
{
    QueueInfo* info = GetQueueInfo(guid);
    if(!info)
        return;

    if(info->requestedToBattle)
    {
        QueueInfo* opInfo = GetQueueInfo(info->opponentGuid);
        if(opInfo)
        {
            opInfo->opponentGuid = NULL;
            opInfo->requestedToBattle = false;
            opInfo->acceptedBattle = false;
            opInfo->acceptedTimer  = 0;
            queueList[info->opponentGuid] = opInfo;

            Player* plr = sObjectAccessor->FindPlayer(info->opponentGuid);
            if(plr)
            {
                plr->PlayerTalkClass->ClearMenus();
                plr->CLOSE_GOSSIP_MENU();
            }
        }
    }

    queueList.erase(guid);
}

void HamarQueueu::Update(uint32 diff)
{
    updateTimer += diff;

    if(updateTimer > QUEUE_UPDATE_INTERVAL)
    {
        // Remove Offline Players

        // Process Queuers
        ProcessQueuePlayers();
        // Process Accepted ones
        ProcessAcceptedQueuePlayers(updateTimer);
        updateTimer = 0;
    }
}

std::map<uint64, QueueInfo*> HamarQueueu::GetQueuers(bool normal)
{
    std::map<uint64, QueueInfo*> result;
    std::map<uint64, QueueInfo*> result2;

    for(std::map<uint64, QueueInfo*>::const_iterator itr = queueList.begin(); itr != queueList.end(); ++itr)
    {
        if(itr->second->requestedToBattle)
            result2.insert(std::pair<uint64, QueueInfo*>(itr->first, itr->second));
        else
            result.insert(std::pair<uint64, QueueInfo*>(itr->first, itr->second));
    }

    return (normal) ? result : result2;
}

void HamarQueueu::RemoveOfflinePlayers()
{
    for(std::map<uint64, QueueInfo*>::const_iterator itr = queueList.begin(); itr != queueList.end(); ++itr)
    {
        Player* plr = sObjectAccessor->FindPlayer(itr->first);

        if(!plr)
            RemovePlayer(itr->first);
    }
}

void HamarQueueu::ProcessQueuePlayers()
{
    std::map<uint64, QueueInfo*> queuePlayers = GetQueuers(true);

    if(queuePlayers.size() < 2)
        return;

    // Take first player from the queueu
    uint64 plrGuid = queuePlayers.begin()->first;
    if(!sObjectAccessor->FindPlayer(plrGuid))
    {
        RemovePlayer(plrGuid);
        return;
    }

    QueueInfo* info = queuePlayers.begin()->second;
    queuePlayers.erase(plrGuid);

    // Take second player from the queueu
    uint64 plrTwoGuid = queuePlayers.begin()->first;
    if(!sObjectAccessor->FindPlayer(plrTwoGuid))
    {
        RemovePlayer(plrTwoGuid);
        return;
    }

    QueueInfo* Opinfo = queuePlayers.begin()->second;
    queuePlayers.erase(plrTwoGuid);

    info->requestedToBattle   = true;
    info->opponentGuid        = plrTwoGuid;
    info->acceptedTimer       = 30*IN_MILLISECONDS;
    Opinfo->requestedToBattle = true;
    Opinfo->acceptedTimer     = 30*IN_MILLISECONDS;
    Opinfo->opponentGuid      = plrGuid;

    // Send menu
    SendBattleRequest(plrGuid);
    SendBattleRequest(plrTwoGuid);
}

void HamarQueueu::ProcessAcceptedQueuePlayers(uint32 diff)
{
    std::map<uint64, QueueInfo*> queuePlayers = GetQueuers(false);

    for(std::map<uint64, QueueInfo*>::const_iterator itr = queuePlayers.begin(); itr != queuePlayers.end(); ++itr)
    {
        QueueInfo * info = itr->second;

        if(!info->acceptedBattle)
        {
            info->acceptedTimer -= diff;

            if(info->acceptedTimer <= IN_MILLISECONDS) // Remove player from queue.
            {
                Player* player = sObjectAccessor->FindPlayer(itr->first);

                QueueInfo* opInfo = GetQueueInfo(info->opponentGuid);
                opInfo->opponentGuid = NULL;
                opInfo->acceptedBattle = false;
                opInfo->requestedToBattle = false;
                opInfo->acceptedTimer = 0;
                queueList[info->opponentGuid] = opInfo;


                info->opponentGuid = NULL;
                info->requestedToBattle = false;
                info->acceptedBattle = false;
                RemovePlayer(itr->first);

                if(player)
                    ChatHandler(player->GetSession()).PSendSysMessage("You have been removed from the queueu : Inactive");

                player->CLOSE_GOSSIP_MENU();
                continue;
            }
        }

        queueList[itr->first] = info;
    }

}

void HamarQueueu::SendBattleRequest(uint64 guid)
{
    Player* player = sObjectAccessor->FindPlayer(guid);

    if(!player)
        return;

    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Accept", GOSSIP_SENDER_MAIN, 1);
    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Decline", GOSSIP_SENDER_MAIN, 2);
    player->SEND_GOSSIP_MENU(1, player->GetGUID());
}

bool HamarQueueu::ReceiveBattleAnswer(Player * player, uint32 sender, uint32 action, uint32 menuId)
{
    if(player->PlayerTalkClass->GetGossipMenu().GetMenuId() != menuId)
        return false;

    player->PlayerTalkClass->ClearMenus();

    player->CLOSE_GOSSIP_MENU();
    // RETURN TRUE WHEN YOU WANT TO BREAK FROM THE METHOD BELOW THIS COMMENT LINE IT'S IMPORTANTO!

    QueueInfo* info = GetQueueInfo(player->GetGUID());
    if(!info)
        return true; // Always return true 

    switch(action)
    {
        case 1:{ // Accept
            if(!info->requestedToBattle)
                return true;
            info->acceptedBattle = true;
            queueList[player->GetGUID()] = info;

            QueueInfo * opInfo = GetQueueInfo(info->opponentGuid);
            if(opInfo)
                if(opInfo->acceptedBattle)
                    sHamarMgr->CreateBattle(player->GetGUID(), info->opponentGuid);

            break;
               }
        case 2:{ // Decline
            RemovePlayer(player->GetGUID());
               }
            break;
    }

    return true;
}

QueueInfo* HamarQueueu::GetQueueInfo(uint64 guid)
{
    std::map<uint64, QueueInfo*>::const_iterator itr = queueList.find(guid);
    if(itr != queueList.end())
        return itr->second;

    return NULL;
}

HamarBattle::HamarBattle() { }

HamarBattle::HamarBattle(Player * plr, Player * plrTwo)
{
    player1         = plr;
    player2         = plrTwo;
    plrOneGuid      = plr->GetGUID();
    plrTwoGuid      = plrTwo->GetGUID();
    m_battleTimer   = 0;
    m_waitTimer     = 20*IN_MILLISECONDS;
    m_status        = HAMAR_WAIT;
    m_startEvent    = EVENT_STARTING_ANNOUNCE_15S;
    map_id          = 1;
    phaseMask       = sHamarMgr->CreatePhaseMask();
    // Create map here!

    player1->pvpBattle = this;
    player2->pvpBattle = this;

    map = sMapMgr->CreateBaseMap(map_id);
    if(!map)
    {
        return;
    }

    if(!SetupBattleGround())
    {
        // Errorz man errorz
        return;
    }

    // Store players locations into array

    // Now teleport players, how every if we change phasemask objects will dissapear ;(
    StorePlayerLocations();
    TeleportPlayers();
}

HamarBattle::~HamarBattle()
{
    m_status           = HAMAR_ENDED;
    player1->pvpBattle = NULL;
    player2->pvpBattle = NULL;
}

void HamarBattle::Update(uint32 diff)
{

    m_checkupTimer += diff;

    if(m_checkupTimer >= 10 * IN_MILLISECONDS)
    {
        m_checkupTimer = 0;

        if(!PlayerOnline(plrOneGuid))
        {
            if(!PlayerOnline(plrTwoGuid))
                EndBattleGround(NULL);
            else
                EndBattleGround(player2);

            return;
        }
        else if(!PlayerOnline(plrTwoGuid))
        {
            if(!PlayerOnline(plrOneGuid))
                EndBattleGround(NULL);
            else
                EndBattleGround(player1);
            return;
        }
    }

    ProcessWait(diff);
}

inline void HamarBattle::ProcessWait(uint32 diff)
{
    m_waitTimer -= diff;
    m_battleTimer += diff;

    if(m_waitTimer <= 15 * IN_MILLISECONDS && m_startEvent == EVENT_STARTING_ANNOUNCE_15S)
    {
        AnnounceToPlayers("15 seconds before battle will begin");
        m_startEvent = EVENT_ANNOUNCE_EVERY_SECOND;
    }

    if(m_waitTimer <= 5 * IN_MILLISECONDS && m_startEvent == EVENT_ANNOUNCE_EVERY_SECOND)
    {
        AnnounceToPlayers("5 seconds before battle will begin");
        m_startEvent = EVENT_START_DESPAWN;
    }

    if(m_waitTimer <= 1 * IN_MILLISECONDS && m_startEvent == EVENT_START_DESPAWN)
    {
        if(DeleteObjects())
            AnnounceToPlayers("Battle has begun!");
        else
            AnnounceToPlayers("There was problem despawning doors");

        m_startEvent = EVENT_COMPLETED;
        m_status     = HAMAR_IN_PROGRESS;
    }

    if(m_battleTimer >= 120*IN_MILLISECONDS && m_status == HAMAR_IN_PROGRESS)
    {
        AnnounceToPlayers("Game took too long, it's a draw!.");
        EndBattleGround(NULL);
    }
}
// Battle System

void HamarBattle::HandlePlayerKill(Player * victim, Player* killer)
{
    if(victim == killer)
        EndBattleGround(NULL);
    else
        EndBattleGround(killer);
}

bool HamarBattle::SetupBattleGround()
{
    if(!AddObject(BG_HAM_OBJECTID_GATE, Door_positions[0][0], Door_positions[0][1], Door_positions[0][2], Door_positions[0][3], Door_positions[0][4], Door_positions[0][5], Door_positions[0][6], Door_positions[0][7], 0)
        || !AddObject(BG_HAM_OBJECTID_GATE, Door_positions[1][0], Door_positions[1][1], Door_positions[1][2], Door_positions[1][3], Door_positions[1][4], Door_positions[1][5], Door_positions[1][6], Door_positions[1][7], 0))
    {
        return false;
    }

    return true;
}

bool HamarBattle::DeleteObjects()
{
    if(objects.empty())
        return false;

    for(BGobjects::iterator itr = objects.begin(); itr != objects.end(); ++itr)
    {
        if(GameObject* go = GetHamarMap()->GetGameObject(*itr))
            go->Delete();
    }

    objects.clear();
    return true;
}

bool HamarBattle::AddObject(uint32 entry, float x, float y, float z, float o, float rotation0, float rotation1, float rotation2, float rotation3, uint32 respawnTime)
{
    Map* map = GetHamarMap();
    if(!map)
        return false;

    GameObject* go = new GameObject;

    if(!go->Create(sObjectMgr->GenerateLowGuid(HIGHGUID_GAMEOBJECT), entry, GetHamarMap(), phaseMask, x, y, z, o, rotation0, rotation1, rotation2, rotation3, 100, GO_STATE_READY))
    {
        delete go;
        return false;
    }


    if(!map->AddToMap(go))
    {
        delete go;
        return false;
    }

    objects.push_back(go->GetGUID());
    return true;
}

void HamarBattle::TeleportPlayers()
{
    if(player1 && player2)
    {
        player1->TeleportTo(GetHamarMap()->GetId(), 2169.34f, -4800.90f, 55.138f, 1.232399f);
        player2->TeleportTo(GetHamarMap()->GetId(), 2205.97f, -4742.49f, 55.138f, 3.808505f);
        player1->SetPhaseMask(phaseMask, true);
        player2->SetPhaseMask(phaseMask, true);
    }
}

void HamarBattle::AnnounceToPlayers(std::string text)
{
    if(PlayerOnline(plrOneGuid))
        ChatHandler(player1->GetSession()).PSendSysMessage(text.c_str());

    if(PlayerOnline(plrTwoGuid))
        ChatHandler(player2->GetSession()).PSendSysMessage(text.c_str());
}

void HamarBattle::EndBattleGround(Player* winner)
{
    if(m_status == HAMAR_ENDED)
        return;

    m_status = HAMAR_ENDED;
    // Teleport players away.

    if(winner && winner->IsInWorld())
    {
        winner->ModifyArenaPoints(3); // 3 arenapoints per win.
        AnnounceToPlayers(winner->GetName() + " has won the game.");
    }

    if(player1 && player1->IsInWorld())
    {

        if(!player1->IsAlive())
            player1->ResurrectPlayer(100.0f);

        player1->TeleportTo(plr1Pos.mapId, plr1Pos.x, plr1Pos.y, plr1Pos.z, plr1Pos.o);
        player1->SetPhaseMask(PHASEMASK_NORMAL, true);
        player1->pvpBattle = NULL;
    }

    if(player2 && player2->IsInWorld())
    {
        if(!player2->IsAlive())
            player2->ResurrectPlayer(100.0f);

        player2->TeleportTo(plr2Pos.mapId, plr2Pos.x, plr2Pos.y, plr2Pos.z, plr2Pos.o);
        player2->SetPhaseMask(PHASEMASK_NORMAL, true);
        player2->pvpBattle = NULL;
    }

    // Despawn All Objects And Return Phase
    
    if(!objects.empty())
        DeleteObjects();

    sHamarMgr->ReturnPhaseMask(phaseMask);
    sHamarMgr->RemoveBattle(this);
}

void HamarBattle::StorePlayerLocations()
{
    plr1Pos.mapId = player1->GetMapId();
    plr1Pos.x     = player1->GetPositionX();
    plr1Pos.y     = player1->GetPositionY();
    plr1Pos.z     = player1->GetPositionZ();
    plr1Pos.o     = player1->GetOrientation();

    plr2Pos.mapId = player2->GetMapId();
    plr2Pos.x     = player2->GetPositionX();
    plr2Pos.y     = player2->GetPositionY();
    plr2Pos.z     = player2->GetPositionZ();
    plr2Pos.o     = player2->GetOrientation();
}

bool HamarBattle::PlayerOnline(uint64 guid)
{
    Player* plr = sObjectAccessor->FindPlayer(guid);

    return (plr) ? true : false;
}

bool HamarBattle::CanUpdate()
{
    if(m_status == HAMAR_ENDED)
        return false;

    return true;
}

void HamarBattleManager::Update(uint32 diff)
{
    if(m_battles.empty())
        return;

    UpdateTimer += diff;

    if(UpdateTimer >= IN_MILLISECONDS)
    {
        for(std::list<HamarBattle*>::const_iterator itr = m_battles.begin(); itr != m_battles.end(); ++itr)
        {
            
            if(m_battles.empty())
                return;

            if(!(*itr))
                continue;

            if((*itr)->CanUpdate())
                (*itr)->Update(UpdateTimer);

        }

        UpdateTimer = 0;
    }
}

bool HamarBattleManager::CreateBattle(uint64 plrGuid, uint64 plr2Guid)
{
    Player * player = sObjectAccessor->FindPlayer(plrGuid);
    Player * secondPlayer = sObjectAccessor->FindPlayer(plr2Guid);

    if(!player || !secondPlayer)
        return false;

    sHamarQueueMgr->RemovePlayer(plrGuid);
    sHamarQueueMgr->RemovePlayer(plr2Guid);

    if(HamarBattle * battle = new HamarBattle(player, secondPlayer))
    {
        m_battles.push_back(battle);
        return true;
    }

    return false;
}

void HamarBattleManager::RemoveBattle(HamarBattle* hm)
{
    m_battles.remove(hm);
}

void HamarBattleManager::CreatePhases()
{
    uint32 phases[20] = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576};

    for(int i = 0; i < 16; i++)
    {
        m_availablePhases.insert(std::pair<uint32, bool>(phases[i], true));
    }
}

uint32 HamarBattleManager::CreatePhaseMask()
{
    for(std::map<uint32, bool>::const_iterator itr = m_availablePhases.begin(); itr != m_availablePhases.end(); ++itr)
    {
        if(!itr->second)
            continue;

        m_availablePhases[itr->first] = false;
        return itr->first;
    }

    return NULL;
}

void HamarBattleManager::ReturnPhaseMask(uint32 phaseMask)
{
    m_availablePhases[phaseMask] = true;
}