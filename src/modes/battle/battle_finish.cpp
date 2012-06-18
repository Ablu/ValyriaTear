////////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2010 by The Allacrost Project
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software and
// you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
////////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    battle_finish.cpp
*** \author  Tyler Olsen, roots@allacrost.org
*** \brief   Source file for battle finish menu
*** ***************************************************************************/

#include "engine/audio/audio.h"
#include "engine/mode_manager.h"
#include "engine/input.h"
#include "engine/system.h"
#include "engine/video/video.h"

#include "modes/battle/battle.h"
#include "modes/battle/battle_actions.h"
#include "modes/battle/battle_actors.h"
#include "modes/battle/battle_finish.h"
#include "modes/battle/battle_utils.h"

#include "modes/boot/boot.h"

using namespace std;

using namespace hoa_utils;

using namespace hoa_audio;
using namespace hoa_video;
using namespace hoa_gui;
using namespace hoa_input;
using namespace hoa_mode_manager;
using namespace hoa_system;
using namespace hoa_global;

namespace hoa_battle {

namespace private_battle {

//! \brief Draw position and dimension constants used for GUI objects
//@{
const float TOP_WINDOW_XPOS        = 512.0f;
const float TOP_WINDOW_YPOS        = 664.0f;
const float TOP_WINDOW_WIDTH       = 512.0f;
const float TOP_WINDOW_HEIGHT      = 64.0f;

const float TOOLTIP_WINDOW_XPOS    = TOP_WINDOW_XPOS;
const float TOOLTIP_WINDOW_YPOS    = TOP_WINDOW_YPOS - TOP_WINDOW_HEIGHT + 16.0f;
const float TOOLTIP_WINDOW_WIDTH   = TOP_WINDOW_WIDTH;
const float TOOLTIP_WINDOW_HEIGHT  = 112.0f;

const float CHAR_WINDOW_XPOS       = TOP_WINDOW_XPOS;
const float CHAR_WINDOW_YPOS       = TOOLTIP_WINDOW_YPOS;
const float CHAR_WINDOW_WIDTH      = TOP_WINDOW_WIDTH;
const float CHAR_WINDOW_HEIGHT     = 120.0f;

const float SPOILS_WINDOW_XPOS     = TOP_WINDOW_XPOS;
const float SPOILS_WINDOW_YPOS     = TOOLTIP_WINDOW_YPOS;
const float SPOILS_WINDOW_WIDTH    = TOP_WINDOW_WIDTH;
const float SPOILS_WINDOW_HEIGHT   = 220.0f;
//@}

////////////////////////////////////////////////////////////////////////////////
// FinishDefeatAssistant class
////////////////////////////////////////////////////////////////////////////////

FinishDefeatAssistant::FinishDefeatAssistant(FINISH_STATE& state) :
	_state(state),
	_retries_left(0)
{
	_options_window.Create(TOP_WINDOW_WIDTH, TOP_WINDOW_HEIGHT, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_BOTTOM);
	_options_window.SetPosition(TOP_WINDOW_XPOS, TOP_WINDOW_YPOS);
	_options_window.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_TOP);
	_options_window.Show();

	_tooltip_window.Create(TOOLTIP_WINDOW_WIDTH, TOOLTIP_WINDOW_HEIGHT);
	_tooltip_window.SetPosition(TOOLTIP_WINDOW_XPOS, TOOLTIP_WINDOW_YPOS);
	_tooltip_window.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_TOP);
	_tooltip_window.Show();

	_options.SetOwner(&_options_window);
	_options.SetPosition(TOP_WINDOW_WIDTH / 2, TOP_WINDOW_HEIGHT / 2 + 4.0f);
	_options.SetDimensions(480.0f, 50.0f, 4, 1, 4, 1);
	_options.SetTextStyle(TextStyle("title22", Color::white, VIDEO_TEXT_SHADOW_DARK));
	_options.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
	_options.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
	_options.SetSelectMode(VIDEO_SELECT_SINGLE);
	_options.SetHorizontalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
	_options.SetCursorOffset(-60.0f, 25.0f);
	_options.AddOption(UTranslate("Retry"));
	_options.AddOption(UTranslate("Restart"));
	_options.AddOption(UTranslate("Return"));
	_options.AddOption(UTranslate("Retire"));
	_options.SetSelection(0);

	_confirm_options.SetOwner(&_options_window);
	_confirm_options.SetPosition(TOP_WINDOW_WIDTH / 2, TOP_WINDOW_HEIGHT / 2 + 4.0f);
	_confirm_options.SetDimensions(240.0f, 50.0f, 2, 1, 2, 1);
	_confirm_options.SetTextStyle(TextStyle("title22", Color::white, VIDEO_TEXT_SHADOW_DARK));
	_confirm_options.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
	_confirm_options.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
	_confirm_options.SetSelectMode(VIDEO_SELECT_SINGLE);
	_confirm_options.SetHorizontalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
	_confirm_options.SetCursorOffset(-60.0f, 25.0f);
	_confirm_options.AddOption(UTranslate("Yes"));
	_confirm_options.AddOption(UTranslate("No"));
	_confirm_options.SetSelection(0);

	_tooltip.SetOwner(&_tooltip_window);
	_tooltip.SetPosition(32.0f, TOOLTIP_WINDOW_HEIGHT - 40.0f);
	_tooltip.SetDimensions(480.0f, 80.0f);
	_tooltip.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_tooltip.SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_tooltip.SetDisplaySpeed(30);
	_tooltip.SetTextStyle(TextStyle("text20", Color::white));
	_tooltip.SetDisplayMode(VIDEO_TEXT_INSTANT);

	// TEMP: disabled because feature is not yet available
	_options.EnableOption(DEFEAT_OPTION_RESTART, false);
}



FinishDefeatAssistant::~FinishDefeatAssistant() {
	_options_window.Destroy();
	_tooltip_window.Destroy();
}



void FinishDefeatAssistant::Initialize(uint32 retries_left) {
	_retries_left = retries_left;

	if (_retries_left == 0) {
		_options.EnableOption(DEFEAT_OPTION_RETRY, false);
	}

	_SetTooltipText();

	_options_window.Show();
	_tooltip_window.Show();
}



void FinishDefeatAssistant::Update() {
	switch (_state) {
		case FINISH_DEFEAT_SELECT:
			if (InputManager->ConfirmPress()) {
				if (_options.IsOptionEnabled(_options.GetSelection()) == false) {
					AudioManager->PlaySound("snd/cancel.wav");
				}
				else {
					_state = FINISH_DEFEAT_CONFIRM;
					_confirm_options.SetSelection(1); // Set default confirm option to "No"
					_SetTooltipText();
				}
			}

			else if (InputManager->LeftPress()) {
				_options.InputLeft();
				_SetTooltipText();
			}
			else if (InputManager->RightPress()) {
				_options.InputRight();
				_SetTooltipText();
			}

			break;

		case FINISH_DEFEAT_CONFIRM:
			if (InputManager->ConfirmPress()) {
				switch (_confirm_options.GetSelection()) {
					case 0: // "Yes"
						_state = FINISH_END;
						_options_window.Hide();
						_tooltip_window.Hide();
						break;
					case 1: // "No"
						_state = FINISH_DEFEAT_SELECT;
						_SetTooltipText();
						break;
					default:
						IF_PRINT_WARNING(BATTLE_DEBUG) << "invalid confirm option selection: " << _confirm_options.GetSelection() << endl;
						break;
				}
			}

			else if (InputManager->CancelPress()) {
				_state = FINISH_DEFEAT_SELECT;
				_SetTooltipText();
			}

			else if (InputManager->LeftPress()) {
				_confirm_options.InputLeft();
			}
			else if (InputManager->RightPress()) {
				_confirm_options.InputRight();
			}

			break;

		case FINISH_END:
			break;

		default:
			IF_PRINT_WARNING(BATTLE_DEBUG) << "invalid finish state: " << _state << endl;
			break;
	}
} // void FinishDefeatAssistant::Update()



void FinishDefeatAssistant::Draw() {
	_options_window.Draw();
	_tooltip_window.Draw();

	if (_state == FINISH_DEFEAT_SELECT) {
		_options.Draw();
	}
	else if (_state == FINISH_DEFEAT_CONFIRM) {
		_confirm_options.Draw();
	}

	_tooltip.Draw();
}



void FinishDefeatAssistant::_SetTooltipText() {
	_tooltip.SetDisplayText("");

	if ((_state == FINISH_ANNOUNCE_RESULT) || (_state == FINISH_DEFEAT_SELECT)) {
		switch (_options.GetSelection()) {
			case DEFEAT_OPTION_RETRY:
				_tooltip.SetDisplayText(Translate("Start over from the beginning of this battle.\nAttempts Remaining: ") + NumberToString(_retries_left));
				break;
			case DEFEAT_OPTION_RESTART:
				_tooltip.SetDisplayText(UTranslate("Load the game from the last save game point."));
				break;
			case DEFEAT_OPTION_RETURN:
				_tooltip.SetDisplayText(UTranslate("Returns the game to the main boot menu."));
				break;
			case DEFEAT_OPTION_RETIRE:
				_tooltip.SetDisplayText(UTranslate("Exit the game."));
				break;
		}
	}
	else if (_state == FINISH_DEFEAT_CONFIRM) {
		switch (_options.GetSelection()) {
			case DEFEAT_OPTION_RETRY:
				_tooltip.SetDisplayText(UTranslate("Confirm: retry battle."));
				break;
			case DEFEAT_OPTION_RESTART:
				_tooltip.SetDisplayText(UTranslate("Confirm: restart from last save."));
				break;
			case DEFEAT_OPTION_RETURN:
				_tooltip.SetDisplayText(UTranslate("Confirm: return to main menu."));
				break;
			case DEFEAT_OPTION_RETIRE:
				_tooltip.SetDisplayText(UTranslate("Confirm: exit game."));
				break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// FinishVictoryAssistant class
////////////////////////////////////////////////////////////////////////////////

FinishVictoryAssistant::FinishVictoryAssistant(FINISH_STATE& state) :
	_state(state),
	_retries_used(0),
	_number_characters(0),
	_xp_earned(0),
	_drunes_dropped(0),
	_begin_counting(false),
	_number_character_windows_created(0)
{
	_header_window.Create(TOP_WINDOW_WIDTH, TOP_WINDOW_HEIGHT, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_BOTTOM);
	_header_window.SetPosition(TOP_WINDOW_XPOS, TOP_WINDOW_YPOS);
	_header_window.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_TOP);
	_header_window.Show();

	// Note: Character windows are created later when the Initialize() function is called. This is done because the borders
	// used with these windows depend on the number of characters in the party.

	_spoils_window.Create(SPOILS_WINDOW_WIDTH, SPOILS_WINDOW_HEIGHT);
	_spoils_window.SetPosition(SPOILS_WINDOW_XPOS, SPOILS_WINDOW_YPOS);
	_spoils_window.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_TOP);
	_spoils_window.Show();

	_header_growth.SetOwner(&_header_window);
	_header_growth.SetPosition(TOP_WINDOW_WIDTH / 2 - 50.0f, TOP_WINDOW_HEIGHT - 20.0f);
	_header_growth.SetDimensions(400.0f, 40.0f);
	_header_growth.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_header_growth.SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_header_growth.SetDisplaySpeed(30);
	_header_growth.SetTextStyle(TextStyle("text20", Color::white));
	_header_growth.SetDisplayMode(VIDEO_TEXT_INSTANT);

	_header_drunes_dropped.SetOwner(&_header_window);
	_header_drunes_dropped.SetPosition(TOP_WINDOW_WIDTH / 2 - 200.0f, TOP_WINDOW_HEIGHT - 20.0f);
	_header_drunes_dropped.SetDimensions(400.0f, 40.0f);
	_header_drunes_dropped.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_header_drunes_dropped.SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_header_drunes_dropped.SetDisplaySpeed(30);
	_header_drunes_dropped.SetTextStyle(TextStyle("text20", Color::white));
	_header_drunes_dropped.SetDisplayMode(VIDEO_TEXT_INSTANT);

	_header_total_drunes.SetOwner(&_header_window);
	_header_total_drunes.SetPosition(TOP_WINDOW_WIDTH / 2 + 50.0f, TOP_WINDOW_HEIGHT - 20.0f);
	_header_total_drunes.SetDimensions(400.0f, 40.0f);
	_header_total_drunes.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_header_total_drunes.SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_header_total_drunes.SetDisplaySpeed(30);
	_header_total_drunes.SetTextStyle(TextStyle("text20", Color::white));
	_header_total_drunes.SetDisplayMode(VIDEO_TEXT_INSTANT);

	for (uint32 i = 0; i < 4; i++) {
		_growth_list[i].SetOwner(&(_character_window[i]));
	}

	_object_header_text.SetOwner(&_spoils_window);
	_object_header_text.SetPosition(SPOILS_WINDOW_WIDTH / 2 - 50.0f, SPOILS_WINDOW_HEIGHT - 10.0f);
	_object_header_text.SetDimensions(200.0f, 40.0f);
	_object_header_text.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_object_header_text.SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_object_header_text.SetDisplaySpeed(30);
	_object_header_text.SetTextStyle(TextStyle("title20", Color::white));
	_object_header_text.SetDisplayMode(VIDEO_TEXT_INSTANT);
	_object_header_text.SetDisplayText(UTranslate("Items Found"));

	_object_list.SetOwner(&_spoils_window);
	_object_list.SetPosition(100.0f, SPOILS_WINDOW_HEIGHT - 35.0f);
	_object_list.SetDimensions(300.0f, 160.0f, 1, 8, 1, 8);
	_object_list.SetTextStyle(TextStyle("text20", Color::white));
	_object_list.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_object_list.SetOptionAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
	_object_list.SetCursorState(VIDEO_CURSOR_STATE_HIDDEN);
}



FinishVictoryAssistant::~FinishVictoryAssistant() {
	_header_window.Destroy();
	_spoils_window.Destroy();

	for (uint32 i = 0; i < _number_character_windows_created; i++) {
		_character_window[i].Destroy();
	}

	// Clear away any skills that characters may have learned so that they do not persist in the growth manager
	for (uint32 i = 0; i < _number_characters; i++) {
		if (_character_growths[i] != NULL) {
			_character_growths[i]->GetSkillsLearned()->clear();
		}
	}

	// Add all the objects that were dropped by enemies to the party's inventory
	for (map<GlobalObject*, int32>::iterator i = _objects_dropped.begin(); i != _objects_dropped.end(); i++) {
		GlobalManager->AddToInventory(i->first->GetID(), i->second);
	}

	// Update the HP and SP of all characters before leaving
	_SetCharacterStatus();
}



void FinishVictoryAssistant::Initialize(uint32 retries_used) {
	_retries_used = retries_used;
	if (_retries_used >= MAX_BATTLE_ATTEMPTS) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "function received invalid argument value: " << retries_used << endl;
		_retries_used = MAX_BATTLE_ATTEMPTS - 1;
	}

	// ----- (1): Prepare all character data
	deque<BattleCharacter*>& all_characters = BattleMode::CurrentInstance()->GetCharacterActors();
	_number_characters = all_characters.size();

	for (uint32 i = 0; i < _number_characters; ++i) {
		_characters.push_back(all_characters[i]->GetGlobalCharacter());
		_character_growths.push_back(_characters[i]->GetGrowth());
		std::string portrait_filename = "img/portraits/map/" + _characters[i]->GetFilename() + ".png";
		if (DoesFileExist(portrait_filename)) {
			_character_portraits[i].Load(portrait_filename, 100.0f, 100.0f);
		}
		else {
			PRINT_WARNING << "Portrait file not found: " << portrait_filename << endl;
			_character_portraits[i].Load("");
		}

		// Grey out portraits of deceased characters
		if (!all_characters[i]->IsAlive())
			_character_portraits[i].EnableGrayScale();
		else
			// Set up the victory animation for the living beings
			all_characters[i]->ChangeSpriteAnimation("victory");
	}

	// ----- (2): Collect the XP, drunes, and dropped objects for each defeated enemy
	deque<BattleEnemy*>& all_enemies = BattleMode::CurrentInstance()->GetEnemyActors();
	GlobalEnemy* enemy;
	vector<GlobalObject*> objects;
	map<GlobalObject*, int32>::iterator iter;

	for (uint32 i = 0; i < all_enemies.size(); ++i)
	{
		enemy = all_enemies[i]->GetGlobalEnemy();
		_xp_earned += enemy->GetExperiencePoints();
		_drunes_dropped += enemy->GetDrunesDropped();
		enemy->DetermineDroppedObjects(objects);

		for (uint32 j = 0; j < objects.size(); ++j)
		{
			// Check if the object to add is already in our list. If so, just increase the quantity of that object.
            // iter = _objects_dropped.find(objects[j]); // Will not work since each item is created with new.
            // Need to search for the item ID instead.
            iter = _objects_dropped.begin();
            while (iter != _objects_dropped.end()) {
                if (iter->first->GetID() == objects[j]->GetID()) break;
                iter++;
            }

			if (iter != _objects_dropped.end())
			{
				iter->second++;
			}
			else
			{
				_objects_dropped.insert(make_pair(objects[j], 1));
			}
		}
	}

	// ----- (3): Divide up the XP and drunes earnings by the number of players (both living and dead) and apply the penalty for any battle retries
	_xp_earned /= _number_characters;

	if (_retries_used > 0) {
		float penalty = 1.0f - (retries_used / MAX_BATTLE_ATTEMPTS);
		_xp_earned = static_cast<uint32>(_xp_earned * penalty);
		_drunes_dropped = static_cast<uint32>(_drunes_dropped * penalty);
	}

	_CreateCharacterGUIObjects();
	_CreateObjectList();
	_SetHeaderText();
} // void FinishVictoryAssistant::Initialize(uint32 retries_used)



void FinishVictoryAssistant::Update() {
	// Update the Battle Character win animation
	std::deque<BattleCharacter*>& all_characters =
		BattleMode::CurrentInstance()->GetCharacterActors();
	std::deque<BattleCharacter*>::iterator it, it_end;

	for (it = all_characters.begin(), it_end = all_characters.end();
			it != it_end; ++it) {
		if ((*it)->IsAlive())
			(*it)->GetGlobalCharacter()->RetrieveBattleAnimation("victory")->Update();
	}

	switch (_state) {
		case FINISH_VICTORY_GROWTH:
			_UpdateGrowth();
			break;

		case FINISH_VICTORY_SPOILS:
			_UpdateSpoils();
			break;

		case FINISH_END:
			break;

		default:
			IF_PRINT_WARNING(BATTLE_DEBUG) << "invalid finish state: " << _state << endl;
			break;
	}
}



void FinishVictoryAssistant::Draw() {
	_header_window.Draw();

	if (_state == FINISH_VICTORY_GROWTH) {
		_header_growth.Draw();
		for (uint32 i = 0; i < _number_characters; i++) {
			_character_window[i].Draw();
			_DrawGrowth(i);
		}
	}
	else if (_state == FINISH_VICTORY_SPOILS) {
		_header_drunes_dropped.Draw();
		_header_total_drunes.Draw();
		_spoils_window.Draw();
		_DrawSpoils();
		_object_list.Draw();
	}
}



void FinishVictoryAssistant::_SetHeaderText() {
	if ((_state == FINISH_ANNOUNCE_RESULT) || (_state == FINISH_VICTORY_GROWTH)) {
		_header_growth.SetDisplayText(UTranslate("XP Earned: ") + MakeUnicodeString(NumberToString(_xp_earned)));
	}
	else if (_state == FINISH_VICTORY_SPOILS) {
		_header_drunes_dropped.SetDisplayText(UTranslate("Drunes Found: ") + MakeUnicodeString(NumberToString(_drunes_dropped)));
		_header_total_drunes.SetDisplayText(UTranslate("Total Drunes: ") + MakeUnicodeString(NumberToString(GlobalManager->GetDrunes())));
	}
	else {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "invalid finish state: " << _state << endl;
	}
}



void FinishVictoryAssistant::_CreateCharacterGUIObjects() {
	// ----- (1): Create the character windows. The lowest one does not have its lower border removed
	float next_ypos = CHAR_WINDOW_YPOS;
	for (uint32 i = 0; i < _number_characters; i++) {
		_number_character_windows_created++;
		if ((i + 1) >= _number_characters) {
			_character_window[i].Create(CHAR_WINDOW_WIDTH, CHAR_WINDOW_HEIGHT);
		}
		else {
			_character_window[i].Create(CHAR_WINDOW_WIDTH, CHAR_WINDOW_HEIGHT, ~VIDEO_MENU_EDGE_BOTTOM, VIDEO_MENU_EDGE_BOTTOM);
		}

		_character_window[i].SetPosition(CHAR_WINDOW_XPOS, next_ypos);
		_character_window[i].SetAlignment(VIDEO_X_CENTER, VIDEO_Y_TOP);
		_character_window[i].Show();
		next_ypos -= CHAR_WINDOW_HEIGHT;
	}

	// ----- (2): Construct GUI objects that will fill each character window
	for (uint32 i = 0; i < _number_characters; i++) {
		_growth_list[i].SetOwner(&_character_window[i]);
		_growth_list[i].SetPosition(290.0f, 115.0f);
		_growth_list[i].SetDimensions(200.0f, 100.0f, 4, 4, 4, 4);
		_growth_list[i].SetTextStyle(TextStyle("text20", Color::white, VIDEO_TEXT_SHADOW_DARK));
		_growth_list[i].SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
		_growth_list[i].SetOptionAlignment(VIDEO_X_RIGHT, VIDEO_Y_CENTER);
		_growth_list[i].SetCursorState(VIDEO_CURSOR_STATE_HIDDEN);
		for (uint32 j = 0; j < 16; j ++) {
			_growth_list[i].AddOption();
		}

		_level_xp_text[i].SetOwner(&_character_window[i]);
		_level_xp_text[i].SetPosition(130.0f, 110.0f);
		_level_xp_text[i].SetDimensions(200.0f, 40.0f);
		_level_xp_text[i].SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
		_level_xp_text[i].SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
		_level_xp_text[i].SetDisplaySpeed(30);
		_level_xp_text[i].SetTextStyle(TextStyle("text20", Color::white));
		_level_xp_text[i].SetDisplayMode(VIDEO_TEXT_INSTANT);
		_level_xp_text[i].SetDisplayText(UTranslate("Level: ") + MakeUnicodeString(NumberToString(_characters[i]->GetExperienceLevel())) +
			MakeUnicodeString("\n") + UTranslate("XP: ") + MakeUnicodeString(NumberToString(_characters[i]->GetExperienceForNextLevel()
			- _characters[i]->GetExperiencePoints())));

		_skill_text[i].SetOwner(&_character_window[i]);
		_skill_text[i].SetPosition(130.0f, 60.0f);
		_skill_text[i].SetDimensions(200.0f, 40.0f);
		_skill_text[i].SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
		_skill_text[i].SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
		_skill_text[i].SetDisplaySpeed(30);
		_skill_text[i].SetTextStyle(TextStyle("text20", Color::white));
		_skill_text[i].SetDisplayMode(VIDEO_TEXT_INSTANT);
	}
} // void FinishVictoryAssistant::_CreateCharacterGUIObjects()



void FinishVictoryAssistant::_CreateObjectList() {
	for (map<hoa_global::GlobalObject*, int32>::iterator i = _objects_dropped.begin(); i != _objects_dropped.end(); i++) {
		GlobalObject* obj = i->first;
		_object_list.AddOption(MakeUnicodeString("<" + obj->GetIconImage().GetFilename() + "><30>")
			+ obj->GetName() + MakeUnicodeString("<R>x" + NumberToString(i->second)));
	}

	// Resize all icon images so that they are the same height as the text
	for (uint32 i = 0; i < _object_list.GetNumberOptions(); i++) {
		_object_list.GetEmbeddedImage(i)->SetDimensions(30.0f, 30.0f);
	}
}



void FinishVictoryAssistant::_SetCharacterStatus() {
	deque<BattleCharacter*>& battle_characters = BattleMode::CurrentInstance()->GetCharacterActors();

	for (deque<BattleCharacter*>::iterator i = battle_characters.begin(); i != battle_characters.end(); i++) {
		GlobalCharacter* character = (*i)->GetGlobalCharacter();

		// Put back the current HP / SP onto the global characters.
		character->SetHitPoints((*i)->GetHitPoints());
		character->SetSkillPoints((*i)->GetSkillPoints());
	}
}



void FinishVictoryAssistant::_UpdateGrowth() {
	// The number of milliseconds that we wait in between updating the XP count
	const uint32 UPDATE_PERIOD = 50;
	// A simple counter used to keep track of when the next XP count should begin
	static uint32 time_counter = 0;

	// The amount of XP to add to each character this update cycle
	uint32 xp_to_add = 0;

	// ---------- (1): Process confirm press inputs.
	if (InputManager->ConfirmPress()) {
		// Begin counting out XP earned
		if (!_begin_counting)
			_begin_counting = true;

		// If confirm received during counting, instantly add all remaining XP
		else if (_xp_earned != 0) {
			xp_to_add = _xp_earned;
		}
		// Counting has finished. Move on to the spoils screen
		else {
			_state = FINISH_VICTORY_SPOILS;
			_SetHeaderText();
		}
	}

	// If counting has not began or counting is alreasy finished, there is nothing more to do here
	if (!_begin_counting || (_xp_earned == 0))
		return;

	// ---------- (2): Update the timer and determine how much XP to add if the time has been reached
	// We don't want to modify the XP to add if a confirm event occurred in step (1)
	if (xp_to_add == 0) {
		time_counter += SystemManager->GetUpdateTime();
		if (time_counter >= UPDATE_PERIOD) {
			time_counter -= UPDATE_PERIOD;

			// Determine an appropriate amount of XP to add here
			if (_xp_earned > 10000)
			    xp_to_add = 1000;
			else if (_xp_earned > 1000)
				xp_to_add = 100;
			else if (_xp_earned > 100)
				xp_to_add = 10;
			else
				xp_to_add = 1;
		}
	}

	// If there is no XP to add this update cycle, there is nothing left for us to do
	if (xp_to_add == 0) {
		return;
	}

	// ---------- (3): Add the XP amount to the characters appropriately
	deque<BattleCharacter*>& battle_characters = BattleMode::CurrentInstance()->GetCharacterActors();
	for (uint32 i = 0; i < _number_characters; i++) {
		// Don't add experience points to dead characters
		if (battle_characters[i]->IsAlive() == false) {
			continue;
		}

		if (_characters[i]->AddExperiencePoints(xp_to_add) == true) {
			do {
				// Only add text for the stats that experienced growth
				uint32 line = 0;

				// HP
				if (_character_growths[i]->GetHitPointsGrowth() > 0) {
					_growth_list[i].SetOptionText(line, UTranslate("HP:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetHitPointsGrowth())));
					line = line + 2;
				}

				// SP
				if (_character_growths[i]->GetSkillPointsGrowth() > 0) {
					_growth_list[i].SetOptionText(line, UTranslate("SP:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetSkillPointsGrowth())));
					line = line + 2;
				}

				// Strength
				if (_character_growths[i]->GetStrengthGrowth() > 0) {
					_growth_list[i].SetOptionText(line, UTranslate("STR:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetStrengthGrowth())));
					line = line + 2;
				}

				// Vigor
				if (_character_growths[i]->GetVigorGrowth() > 0) {
					_growth_list[i].SetOptionText(line, UTranslate("VIG:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetVigorGrowth())));
					line = line + 2;
				}

				// Fortitude
				if (_character_growths[i]->GetFortitudeGrowth() > 0) {
					_growth_list[i].SetOptionText(line, UTranslate("FOR:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetFortitudeGrowth())));
					line = line + 2;
				}

				// Protection
				if (_character_growths[i]->GetProtectionGrowth() > 0) {
					_growth_list[i].SetOptionText(line, UTranslate("PRO:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetProtectionGrowth())));
					line = line + 2;
				}

				// Agility
				if (_character_growths[i]->GetAgilityGrowth() > 0) {
					_growth_list[i].SetOptionText(line, UTranslate("AGI:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetAgilityGrowth())));
					line = line + 2;
				}

				// Evade
				if (_character_growths[i]->GetEvadeGrowth() > 0.001f) {
					_growth_list[i].SetOptionText(line, UTranslate("EVA:"));
					_growth_list[i].SetOptionText(line + 1, MakeUnicodeString(NumberToString(_character_growths[i]->GetEvadeGrowth())));
					line = line + 2;
				}
				_character_growths[i]->AcknowledgeGrowth();
			} while (_character_growths[i]->IsGrowthDetected());

			std::vector<GlobalSkill*>* new_skills = _character_growths[i]->GetSkillsLearned();
			if (new_skills->empty() == false) {
				_skill_text[i].SetDisplayText(UTranslate("New Skill Learned:\n   ") + new_skills->at(0)->GetName());
			}
		}

		// TODO: check for new experience level
		_level_xp_text[i].SetDisplayText(Translate("Level: ") + NumberToString(_characters[i]->GetExperienceLevel()) +
			"\n" + Translate("XP: ") + NumberToString(_characters[i]->GetExperienceForNextLevel() - _characters[i]->GetExperiencePoints()));
	}

	_xp_earned -= xp_to_add;
	_SetHeaderText();
} // void FinishVictoryAssistant::_UpdateGrowth()



void FinishVictoryAssistant::_UpdateSpoils() {
	// The number of milliseconds that we wait in between updating the drunes count
	const uint32 UPDATE_PERIOD = 50;
	// A simple counter used to keep track of when the next drunes count should begin
	static uint32 time_counter = 0;
	// TODO: Add drunes gradually instead of all at once
	static uint32 drunes_to_add = 0;

	// ---------- (1): Process confirm press inputs.
	if (InputManager->ConfirmPress()) {
		// Begin counting out drunes dropped
		if (!_begin_counting)
			_begin_counting = true;

		// If confirm received during counting, instantly add all remaining drunes
		else if (_drunes_dropped != 0) {
			drunes_to_add = _drunes_dropped;
		}
		// Counting is done. Finish supervisor should now terminate
		else {
			_state = FINISH_END;
		}
	}

	// If counting has not began or counting is alreasy finished, there is nothing more to do here
	if (!_begin_counting || (_drunes_dropped == 0))
		return;

	// ---------- (2): Update the timer and determine how many drunes to add if the time has been reached
	// We don't want to modify the drunes to add if a confirm event occurred in step (1)
	if (drunes_to_add == 0) {
		time_counter += SystemManager->GetUpdateTime();
		if (time_counter >= UPDATE_PERIOD) {
			time_counter -= UPDATE_PERIOD;

			// Determine an appropriate amount of drunes to add here
			if (_drunes_dropped > 10000)
			    drunes_to_add = 1000;
			else if (_drunes_dropped > 1000)
				drunes_to_add = 100;
			else if (_drunes_dropped > 100)
				drunes_to_add = 10;
			else
				drunes_to_add = 1;
		}
	}

	// ---------- (2): Add drunes and update the display
	if (drunes_to_add != 0) {
		// Avoid making _drunes_dropped a negative value
		if (drunes_to_add > _drunes_dropped) {
			drunes_to_add = _drunes_dropped;
		}

		GlobalManager->AddDrunes(drunes_to_add);
		_drunes_dropped -= drunes_to_add;
		_SetHeaderText();
	}
} // void FinishVictoryAssistant::_UpdateSpoils()



void FinishVictoryAssistant::_DrawGrowth(uint32 index) {
	VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_TOP, 0);
	VideoManager->Move(CHAR_WINDOW_XPOS - (CHAR_WINDOW_WIDTH / 2) + 20.0f, (CHAR_WINDOW_YPOS - 15.0f) - (CHAR_WINDOW_HEIGHT * index));
	_character_portraits[index].Draw();

	_level_xp_text[index].Draw();
	_growth_list[index].Draw();
	_skill_text[index].Draw();
}



void FinishVictoryAssistant::_DrawSpoils() {
	_object_header_text.Draw();
	_object_list.Draw();
}

////////////////////////////////////////////////////////////////////////////////
// FinishSupervisor class
////////////////////////////////////////////////////////////////////////////////

FinishSupervisor::FinishSupervisor() :
	_state(FINISH_INVALID),
	_attempt_number(0),
	_battle_victory(false),
	_defeat_assistant(_state),
	_victory_assistant(_state)
{
	_outcome_text.SetPosition(400.0f, 700.0f);
	_outcome_text.SetAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_outcome_text.SetTextAlignment(VIDEO_X_LEFT, VIDEO_Y_TOP);
	_outcome_text.SetDimensions(400.0f, 50.0f);
	_outcome_text.SetDisplaySpeed(30);
	_outcome_text.SetTextStyle(TextStyle("text24", Color::white));
	_outcome_text.SetDisplayMode(VIDEO_TEXT_INSTANT);
}



FinishSupervisor::~FinishSupervisor() {

}



void FinishSupervisor::Initialize(bool victory) {
	if (_attempt_number >= MAX_BATTLE_ATTEMPTS) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "exceeded maximum allowed number of battle attempts" << endl;
	}
	else {
		_attempt_number++;
	}

	_battle_victory = victory;
	_state = FINISH_ANNOUNCE_RESULT;

	if (_battle_victory == true) {
		_victory_assistant.Initialize(_attempt_number - 1);
		_outcome_text.SetDisplayText(UTranslate("The heroes were victorious!"));
	}
	else {
		_defeat_assistant.Initialize(MAX_BATTLE_ATTEMPTS - _attempt_number);
		_outcome_text.SetDisplayText(UTranslate("But the heroes fell in battle..."));
	}
}



void FinishSupervisor::Update() {
	if (_state == FINISH_ANNOUNCE_RESULT) {
		if (_battle_victory == true) {
			_state = FINISH_VICTORY_GROWTH;
		}
		else {
			_state = FINISH_DEFEAT_SELECT;
		}
		return;
	}

	if (_battle_victory == true) {
		_victory_assistant.Update();
	}
	else {
		_defeat_assistant.Update();
	}

	if (_state == FINISH_END) {
		if (_battle_victory) {
			BattleMode::CurrentInstance()->ChangeState(BATTLE_STATE_EXITING);
		}

		else {
			switch (_defeat_assistant.GetDefeatOption()) {
				case DEFEAT_OPTION_RETRY:
					BattleMode::CurrentInstance()->RestartBattle();
					break;
				case DEFEAT_OPTION_RESTART:
					// TODO: Load last saved game
					break;
				case DEFEAT_OPTION_RETURN:
					ModeManager->PopAll();
					ModeManager->Push(new hoa_boot::BootMode(), true, true);
					break;
				case DEFEAT_OPTION_RETIRE:
					SystemManager->ExitGame();
					break;
				default:
					IF_PRINT_WARNING(BATTLE_DEBUG) << "invalid defeat option selected: " << _defeat_assistant.GetDefeatOption() << endl;
					break;
			}
		}
	}
}



void FinishSupervisor::Draw() {
	_outcome_text.Draw();

	if (_battle_victory == true) {
		_victory_assistant.Draw();
	}
	else {
		_defeat_assistant.Draw();
	}
}

} // namespace private_battle

} // namespace hoa_battle
