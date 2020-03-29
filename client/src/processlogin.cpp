/*
 * =====================================================================================
 *
 *       Filename: processlogin.cpp
 *        Created: 08/14/2015 02:47:49
 *    Description: 
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#include <cstring>
#include <iostream>
#include <algorithm>

#include "log.hpp"
#include "client.hpp"
#include "message.hpp"
#include "pngtexdb.hpp"
#include "sdldevice.hpp"
#include "processlogin.hpp"

extern Log *g_Log;
extern Client *g_Client;
extern PNGTexDB *g_ProgUseDB;
extern SDLDevice *g_SDLDevice;

ProcessLogin::ProcessLogin()
	: Process()
	, m_Button1(150, 482, 0X00000005, []{}, [this](){ DoCreateAccount(); })
	, m_Button2(352, 482, 0X00000008, []{}, [    ](){                    })
	, m_Button3(554, 482, 0X0000000B, []{}, [    ](){ std::exit(0);      })
    , m_Button4(600, 536, 0X0000000E, []{}, [this](){ DoLogin();         })
	, m_IDBox(
            159,
            540,
            146,
            18,
            2,
            1,
            14,
            {0XFF, 0XFF, 0XFF, 0XFF},
            {0XFF, 0XFF, 0XFF, 0XFF},
            [this]()
            {
                m_IDBox      .focus(false);
                m_PasswordBox.focus(true);
            },
            [this]()
            {
                DoLogin();
            })
	, m_PasswordBox(
            409,
            540,
            146,
            18,
            true,
            2,
            1,
            14,
            {0XFF, 0XFF, 0XFF, 0XFF},
            {0XFF, 0XFF, 0XFF, 0XFF},
            [this]()
            {
                m_IDBox      .focus(true);
                m_PasswordBox.focus(false);
            },
            [this]()
            {
                DoLogin();
            })
{}

void ProcessLogin::Update(double fMS)
{
    m_IDBox.Update(fMS);
    m_PasswordBox.Update(fMS);
}

void ProcessLogin::Draw()
{
    g_SDLDevice->ClearScreen();

    g_SDLDevice->DrawTexture(g_ProgUseDB->Retrieve(0X00000003),   0,  75);
    g_SDLDevice->DrawTexture(g_ProgUseDB->Retrieve(0X00000004),   0, 465);
    g_SDLDevice->DrawTexture(g_ProgUseDB->Retrieve(0X00000011), 103, 536);

    m_Button1.draw();
    m_Button2.draw();
    m_Button3.draw();
    m_Button4.draw();

    m_IDBox      .draw();
    m_PasswordBox.draw();

    g_SDLDevice->Present();
}

void ProcessLogin::processEvent(const SDL_Event &event)
{
    switch(event.type){
        case SDL_KEYDOWN:
            {
                switch(event.key.keysym.sym){
                    case SDLK_TAB:
                        {
                            if(true
                                    && !m_IDBox      .focus()
                                    && !m_PasswordBox.focus()){

                                m_IDBox      .focus(true);
                                m_PasswordBox.focus(false);
                                return;
                            }
                        }
                    default:
                        {
                            break;
                        }
                }
            }
        default:
            {
                break;
            }
    }

    m_Button1.processEvent(event, true);
    m_Button2.processEvent(event, true);
    m_Button3.processEvent(event, true);
    m_Button4.processEvent(event, true);

    // widget idbox and pwdbox are not independent from each other
    // tab in one box will grant focus to another

    m_IDBox      .processEvent(event, true);
    m_PasswordBox.processEvent(event, true);
}

void ProcessLogin::DoLogin()
{
    if(!(m_IDBox.Content().empty()) && !(m_PasswordBox.Content().empty())){
        g_Log->AddLog(LOGTYPE_INFO, "login account: (%s:%s)", m_IDBox.Content().c_str(), m_PasswordBox.Content().c_str());

        auto szID  = m_IDBox.Content();
        auto szPWD = m_PasswordBox.Content();

        CMLogin stCML;
        std::memset(&stCML, 0, sizeof(stCML));

        if((szID.size() >= sizeof(stCML.ID)) || (szPWD.size() >= sizeof(stCML.Password))){
            g_Log->AddLog(LOGTYPE_WARNING, "Too long ID/PWD provided");
            return;
        }

        std::memcpy(stCML.ID, szID.c_str(), szID.size());
        std::memcpy(stCML.Password, szPWD.c_str(), szPWD.size());

        g_Client->Send(CM_LOGIN, stCML);
    }
}

void ProcessLogin::DoCreateAccount()
{
    g_Client->RequestProcess(PROCESSID_NEW);
}
