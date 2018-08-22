#include "Brain.h"

#include <iostream>

#include "Utils.h"

void Brain::Init()
{
    m_hands.Delay(500);
    m_hands.ResetUI();
    m_hands.ResetCamera();
    m_hands.Send(500);
}

void Brain::Process()
{
    m_npcs = m_eyes.DetectNPCs();
    m_far_npcs = m_eyes.DetectFarNPCs();
    m_me = m_eyes.DetectMe();
    m_target = m_eyes.DetectTarget();

    if (!m_me.has_value()) {
        return;
    }

    const auto me = m_me.value();

    if (me.hp < 70 && !LOCKED(1000)) {
        std::cout << "Restore HP" << std::endl;
        m_hands.RestoreHP();
        m_hands.Send();
    }

    if (me.mp < 70 && !LOCKED(1000)) {
        std::cout << "Restore MP" << std::endl;
        m_hands.RestoreMP();
        m_hands.Send();
    }

    if (me.cp < 90 && !LOCKED(1000)) {
        std::cout << "Restore CP" << std::endl;
        m_hands.RestoreCP();
        m_hands.Send();
    }

    const auto target = m_target.value_or(::Eyes::Target{});

    if (target.hp > 0) {
        m_state = State::Attack;
    }

    if (!m_hands.IsReady()) {
        return;
    }

    if (m_state == State::NextTarget) {
        std::cout << "Next target" << std::endl;
        m_hands.NextTarget();
        m_hands.Send(500);
        m_state = State::NearSearch;
    } else if (m_state == State::NearSearch) {
        const auto npc = UnselectedNPC();

        if (npc != nullptr) {
            std::cout << "Near search: Found NPC" << std::endl;
            IgnoreNPC(npc->Id());
            m_hands.MoveMouseTo({npc->center.x, npc->center.y});
            m_hands.Send(500);
            m_previous_state = m_state;
            m_state = State::Check;
        } else if (m_search_attempt < m_search_attempts) {
            ++m_search_attempt;
            std::cout << "Near search: Look around attempt " << m_search_attempt << std::endl;
            m_hands.LookAround();
            m_hands.NextTarget();
            m_hands.Send(500);
            ClearIgnoredNPCs();
        } else if (!LOCKED(2000)) {
            m_state = State::FarSearch;
            m_search_attempt = 0;
        }
    } else if (m_state == State::FarSearch) {
        const auto npc = FarNPC();

        if (npc != nullptr) {
            std::cout << "Far search: Found NPC" << std::endl;
            IgnoreNPC(npc->Id());
            m_hands.MoveMouseTo({npc->center.x, npc->center.y});
            m_hands.Send(500);
            m_previous_state = m_state;
            m_state = State::Check;
        } else if (m_search_attempt < m_search_attempts) {
            ++m_search_attempt;
            std::cout << "Far search: Look around attempt " << m_search_attempt << std::endl;
            m_hands.LookAround();
            m_hands.NextTarget();
            m_hands.Send(2000);
            ClearIgnoredNPCs();
        } else {
            m_state = State::NextTarget;
            m_search_attempt = 0;
        }
    } else if (m_state == State::Check) {
        std::cout << "Check NPC" << std::endl;
        const auto npc = HoveredNPC();

        if (npc != nullptr) {
            m_hands.SelectTarget();
            m_hands.Send(500);
        }

        m_state = m_previous_state;
    } else if (m_state == State::Attack) {
        if (target.hp > 0) {
            if (m_first_attack) {
                std::cout << "Attack NPC" << std::endl;
                m_first_attack = false;
                m_search_attempt = 0;
                ClearIgnoredNPCs();
                m_hands.Spoil();
                m_hands.Attack();
                m_hands.Delay(500);
                m_hands.ResetCamera();
                m_hands.Send();
            } else {
                m_hands.Attack();
                m_hands.Send(250);
            }
        } else if (!LOCKED(1000)) {
            m_first_attack = true;
            const auto npc = SelectedNPC();

            if (npc != nullptr) {
                std::cout << "Go to NPC" << std::endl;
                m_hands.GoTo({npc->center.x, npc->center.y});
                m_hands.Send(4000); // TODO: moving of the selected target can be detected
            }

            m_state = State::PickUp;
        }
    } else if (m_state == State::PickUp) {
        std::cout << "Pick up loot" << std::endl;
        m_hands.Sweep();
        m_hands.Delay(500);
        m_hands.CancelTarget();
        m_hands.PickUp();
        m_hands.Send();
        m_state = State::NextTarget;
    }
}

const ::Eyes::NPC *Brain::UnselectedNPC() const
{
    const auto npcs = FilteredNPCs();

    for (const auto &npc : npcs) {
        if (!npc->Selected()) {
            return npc;
        }
    }

    return nullptr;
}

const ::Eyes::NPC *Brain::SelectedNPC() const
{
    const auto npcs = FilteredNPCs();

    for (const auto &npc : npcs) {
        if (npc->Selected()) {
            return npc;
        }
    }

    return nullptr;
}

const ::Eyes::NPC *Brain::HoveredNPC() const
{
    for (const auto &npc : m_npcs) {
        if (npc.Hovered()) {
            return &npc;
        }
    }

    return nullptr;
}

const ::Eyes::FarNPC *Brain::FarNPC() const
{
    for (const auto &npc : m_far_npcs) {
        if (m_ignored_npc_ids.find(npc.Id()) == m_ignored_npc_ids.end()) {
            return &npc;
        }
    }

    return nullptr;
}

std::vector<const ::Eyes::NPC *> Brain::FilteredNPCs() const
{
    std::vector<const ::Eyes::NPC *> npcs;

    for (const auto &npc : m_npcs) {
        if (m_ignored_npc_ids.find(npc.Id()) == m_ignored_npc_ids.end()) {
            npcs.push_back(&npc);
        }
    }

    return npcs;
}
