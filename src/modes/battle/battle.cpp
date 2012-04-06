////////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2010 by The Allacrost Project
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software and
// you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
////////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    battle.cpp
*** \author  Tyler Olsen, roots@allacrost.org
*** \author  Viljami Korhonen, mindflayer@allacrost.org
*** \author  Corey Hoffstein, visage@allacrost.org
*** \author  Andy Gardner, chopperdave@allacrost.org
*** \brief   Source file for battle mode interface.
*** ***************************************************************************/

#include "engine/audio/audio.h"
#include "engine/input.h"
#include "engine/mode_manager.h"
#include "engine/script/script.h"
#include "engine/video/video.h"

#include "modes/pause.h"

#include "modes/battle/battle.h"
#include "modes/battle/battle_actors.h"
#include "modes/battle/battle_actions.h"
#include "modes/battle/battle_command.h"
#include "modes/battle/battle_dialogue.h"
#include "modes/battle/battle_finish.h"
#include "modes/battle/battle_sequence.h"
#include "modes/battle/battle_utils.h"

using namespace std;

using namespace hoa_utils;
using namespace hoa_audio;
using namespace hoa_video;
using namespace hoa_mode_manager;
using namespace hoa_input;
using namespace hoa_system;
using namespace hoa_global;
using namespace hoa_script;
using namespace hoa_pause;

using namespace hoa_battle::private_battle;

namespace hoa_battle {

bool BATTLE_DEBUG = false;

// Initialize static class variable
BattleMode* BattleMode::_current_instance = NULL;

namespace private_battle {

////////////////////////////////////////////////////////////////////////////////
// BattleMedia class
////////////////////////////////////////////////////////////////////////////////

// Filenames of the default music that is played when no specific music is requested
//@{
const char* DEFAULT_BATTLE_MUSIC   = "mus/Confrontation.ogg";
const char* DEFAULT_VICTORY_MUSIC  = "mus/Allacrost_Fanfare.ogg";
const char* DEFAULT_DEFEAT_MUSIC   = "mus/Allacrost_Intermission.ogg";
//@}

BattleMedia::BattleMedia() {
	if (!background_image.Load("img/backdrops/battle/desert_cave/desert_cave.png"))
		PRINT_ERROR << "failed to load default background image" << endl;

	if (stamina_icon_selected.Load("img/menus/stamina_icon_selected.png") == false)
		PRINT_ERROR << "failed to load stamina icon selected image" << endl;

	attack_point_indicator.SetDimensions(16.0f, 16.0f);
	if (attack_point_indicator.LoadFromFrameGrid("img/icons/battle/attack_point_target.png", vector<uint32>(4, 100), 1, 4) == false)
		PRINT_ERROR << "failed to load attack point indicator." << endl;

	if (stamina_meter.Load("img/menus/stamina_bar.png") == false)
		PRINT_ERROR << "failed to load time meter." << endl;

	if (actor_selection_image.Load("img/icons/battle/character_selector.png") == false)
		PRINT_ERROR << "unable to load player selector image" << endl;

	if (character_selected_highlight.Load("img/menus/battle_character_selection.png") == false)
		PRINT_ERROR << "failed to load character selection highlight image" << endl;

	if (character_command_highlight.Load("img/menus/battle_character_command.png") == false)
		PRINT_ERROR << "failed to load character command highlight image" << endl;

	if (character_bar_covers.Load("img/menus/battle_character_bars.png") == false)
		PRINT_ERROR << "failed to load character bars image" << endl;

	if (bottom_menu_image.Load("img/menus/battle_bottom_menu.png") == false)
		PRINT_ERROR << "failed to load bottom menu image" << endl;

	if (swap_icon.Load("img/icons/battle/swap_icon.png") == false)
		PRINT_ERROR << "failed to load swap icon" << endl;

	if (swap_card.Load("img/icons/battle/swap_card.png") == false)
		PRINT_ERROR << "failed to load swap card" << endl;

	if (ImageDescriptor::LoadMultiImageFromElementGrid(character_action_buttons, "img/menus/battle_command_buttons.png", 2, 5) == false)
		PRINT_ERROR << "failed to load character action buttons" << endl;

	if (ImageDescriptor::LoadMultiImageFromElementGrid(_target_type_icons, "img/icons/effects/targets.png", 1, 8) == false)
		PRINT_ERROR << "failed to load character action buttons" << endl;

	if (ImageDescriptor::LoadMultiImageFromElementSize(_status_icons, "img/icons/effects/status.png", 25, 25) == false)
		PRINT_ERROR << "failed to load status icon images" << endl;

	if (victory_music.LoadAudio(DEFAULT_VICTORY_MUSIC) == false)
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load victory music file: " << DEFAULT_VICTORY_MUSIC << endl;

	if (defeat_music.LoadAudio(DEFAULT_DEFEAT_MUSIC) == false)
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load defeat music file: " << DEFAULT_DEFEAT_MUSIC << endl;

	if (confirm_sound.LoadAudio("snd/confirm.wav") == false)
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load confirm sound" << endl;

	if (cancel_sound.LoadAudio("snd/cancel.wav") == false)
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load cancel sound" << endl;

	if (cursor_sound.LoadAudio("snd/confirm.wav") == false)
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load cursor sound" << endl;

	if (invalid_sound.LoadAudio("snd/cancel.wav") == false)
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load invalid sound" << endl;

	if (finish_sound.LoadAudio("snd/confirm.wav") == false)
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load finish sound" << endl;

	// Determine which status effects correspond to which icons and store the result in the _status_indices container
	ReadScriptDescriptor& script_file = GlobalManager->GetStatusEffectsScript();

	vector<int32> status_types;
	script_file.ReadTableKeys(status_types);

	for (uint32 i = 0; i < status_types.size(); i++) {
		GLOBAL_STATUS status = static_cast<GLOBAL_STATUS>(status_types[i]);

		// Check for duplicate entries of the same status effect
		if (_status_indeces.find(status) != _status_indeces.end()) {
			IF_PRINT_WARNING(BATTLE_DEBUG) << "duplicate entry found in file " << script_file.GetFilename() <<
				" for status type: " << status_types[i] << endl;
			continue;
		}

		script_file.OpenTable(status_types[i]);
		if (script_file.DoesIntExist("icon_index") == true) {
			uint32 icon_index = script_file.ReadUInt("icon_index");
			_status_indeces.insert(pair<GLOBAL_STATUS, uint32>(status, icon_index));
		}
		else {
			IF_PRINT_WARNING(BATTLE_DEBUG) << "no icon_index member was found for status effect: " << status_types[i] << endl;
		}
		script_file.CloseTable();
	}
}



BattleMedia::~BattleMedia() {
	battle_music.FreeAudio();
	victory_music.FreeAudio();
	defeat_music.FreeAudio();
}


void BattleMedia::Update() {
	attack_point_indicator.Update();

	// Update custom animations
	for (uint32 i = 0; i < _custom_battle_animations.size(); ++i) {
	    _custom_battle_animations[i].Update();
	}
}


void BattleMedia::SetBackgroundImage(const string& filename) {
	if (background_image.Load(filename) == false) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load background image: " << filename << endl;
	}
}


int32 BattleMedia::AddCustomAnimation(const std::string& filename) {
	AnimatedImage anim;
	if (!anim.LoadFromAnimationScript(filename)) {
		PRINT_WARNING << "Custom animation file could not be loaded: " << filename << endl;
		return -1;
	}

	_custom_battle_animations.push_back(anim);

	int32 id = _custom_battle_animations.size() - 1;
	return id;
}


void BattleMedia::DrawCustomAnimation(int32 id, float x, float y) {
	if (id < 0 || id > static_cast<int32>(_custom_battle_animations.size()) - 1)
		return;

	VideoManager->Move(x, y);
	_custom_battle_animations[id].Draw();
}


int32 BattleMedia::AddCustomImage(const std::string& filename, float width,
									float height) {
	StillImage img;
	if (!img.Load(filename, width, height)) {
		PRINT_WARNING << "Custom image file could not be loaded: " << filename << endl;
		return -1;
	}

	_custom_battle_images.push_back(img);

	int32 id = _custom_battle_images.size() - 1;
	return id;
}

void BattleMedia::DrawCustomImage(int32 id, float x, float y, Color color) {
	if (id < 0 || id > static_cast<int32>(_custom_battle_images.size()) - 1)
		return;

	VideoManager->Move(x, y);
	_custom_battle_images[id].Draw(color);
}


void BattleMedia::SetBattleMusic(const string& filename) {
	if (battle_music.LoadAudio(filename) == false) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load music file: " << filename << endl;
	}
}



StillImage* BattleMedia:: GetCharacterActionButton(uint32 index) {
	if (index >= character_action_buttons.size()) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "function received invalid index argument: " << index << endl;
		return NULL;
	}

	return &(character_action_buttons[index]);
}



StillImage* BattleMedia::GetTargetTypeIcon(hoa_global::GLOBAL_TARGET target_type) {
	switch (target_type) {
		case GLOBAL_TARGET_SELF_POINT:
			return &_target_type_icons[0];
		case GLOBAL_TARGET_ALLY_POINT:
			return &_target_type_icons[1];
		case GLOBAL_TARGET_FOE_POINT:
			return &_target_type_icons[2];
		case GLOBAL_TARGET_SELF:
			return &_target_type_icons[3];
		case GLOBAL_TARGET_ALLY:
			return &_target_type_icons[4];
		case GLOBAL_TARGET_FOE:
			return &_target_type_icons[5];
		case GLOBAL_TARGET_ALL_ALLIES:
			return &_target_type_icons[6];
		case GLOBAL_TARGET_ALL_FOES:
			return &_target_type_icons[7];
		default:
			IF_PRINT_WARNING(BATTLE_DEBUG) << "function received invalid target type argument: " << target_type << endl;
			return NULL;
	}
}



StillImage* BattleMedia::GetStatusIcon(GLOBAL_STATUS type, GLOBAL_INTENSITY intensity) {
	if ((type <= GLOBAL_STATUS_INVALID) || (type >= GLOBAL_STATUS_TOTAL)) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "type argument was invalid: " << type << endl;
		return NULL;
	}
	if ((intensity < GLOBAL_INTENSITY_NEUTRAL) || (intensity >= GLOBAL_INTENSITY_TOTAL)) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "type argument was invalid: " << intensity << endl;
		return NULL;
	}

	map<GLOBAL_STATUS, uint32>::iterator status_entry = _status_indeces.find(type);
	if (status_entry == _status_indeces.end()) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "no entry in the status icon index for status type: " << type << endl;
		return NULL;
	}

	uint32 status_index = status_entry->second;
	uint32 intensity_index = static_cast<uint32>(intensity);
	return &(_status_icons[(status_index * 5) + intensity_index]); // TODO: use an appropriate constant instead of the "5" value here
}

} // namespace private_battle

////////////////////////////////////////////////////////////////////////////////
// BattleMode class -- primary methods
////////////////////////////////////////////////////////////////////////////////

BattleMode::BattleMode() :
	_state(BATTLE_STATE_INVALID),
	_sequence_supervisor(NULL),
	_command_supervisor(NULL),
	_dialogue_supervisor(NULL),
	_finish_supervisor(NULL),
	_current_number_swaps(0),
	_last_enemy_dying(false)
{
	IF_PRINT_DEBUG(BATTLE_DEBUG) << "constructor invoked" << endl;

	mode_type = MODE_MANAGER_BATTLE_MODE;

	// Check that the global manager has a valid battle setting stored.
	if ((GlobalManager->GetBattleSetting() <= GLOBAL_BATTLE_INVALID) || (GlobalManager->GetBattleSetting() >= GLOBAL_BATTLE_TOTAL)) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "global manager had invalid battle setting active, changing setting to GLOBAL_BATTLE_WAIT" << endl;
		GlobalManager->SetBattleSetting(GLOBAL_BATTLE_WAIT);
	}

	_sequence_supervisor = new SequenceSupervisor(this);
	_command_supervisor = new CommandSupervisor();
	_dialogue_supervisor = new DialogueSupervisor();
	_finish_supervisor = new FinishSupervisor();
} // BattleMode::BattleMode()



BattleMode::~BattleMode() {
	delete _sequence_supervisor;
	delete _command_supervisor;
	delete _dialogue_supervisor;
	delete _finish_supervisor;

	// Delete all character and enemy actors
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		delete _character_actors[i];
	}
	_character_actors.clear();
	_character_party.clear();

	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		delete _enemy_actors[i];
	}
	_enemy_actors.clear();
	_enemy_party.clear();

	_ready_queue.clear();

	if (_current_instance == this) {
		_current_instance = NULL;
	}
} // BattleMode::~BattleMode()



void BattleMode::Reset() {
	_current_instance = this;

	// Disable potential previous overlay effects
	VideoManager->DisableFadeEffect();

	VideoManager->SetCoordSys(0.0f, VIDEO_STANDARD_RES_WIDTH, 0.0f, VIDEO_STANDARD_RES_HEIGHT);

	// Load the default battle music track if no other music has been added
	if (_battle_media.battle_music.GetState() == AUDIO_STATE_UNLOADED) {
		if (_battle_media.battle_music.LoadAudio(DEFAULT_BATTLE_MUSIC) == false) {
			IF_PRINT_WARNING(BATTLE_DEBUG) << "failed to load default battle music: " << DEFAULT_BATTLE_MUSIC << endl;
		}
	}

	_battle_media.battle_music.Play();

	_last_enemy_dying = false;

	if (_state == BATTLE_STATE_INVALID) {
		_Initialize();
	}

	UnFreezeTimers();
}



void BattleMode::Update() {
	// Update potential battle animations
	_battle_media.Update();

	// Pause/quit requests take priority
	if (InputManager->QuitPress()) {
		ModeManager->Push(new PauseMode(true));
		return;
	}
	if (InputManager->PausePress()) {
		ModeManager->Push(new PauseMode(false));
		return;
	}

	for (uint32 i = 0; i < _update_functions.size(); ++i)
		ReadScriptDescriptor::RunScriptObject(_update_functions[i]);

	if (_dialogue_supervisor->IsDialogueActive() == true) {
		_dialogue_supervisor->Update();

		// Because the dialogue may have ended in the call to Update(), we have to check it again here.
		if (_dialogue_supervisor->IsDialogueActive() == true) {
			if (_dialogue_supervisor->GetCurrentDialogue()->IsHaltBattleAction() == true) {
				return;
			}
		}
	}

	// If the battle is transitioning to/from a different mode, the sequence supervisor has control
	if (_state == BATTLE_STATE_INITIAL || _state == BATTLE_STATE_EXITING) {
		_sequence_supervisor->Update();
		return;
	}
	// If the battle is in its typical state and player is not selecting a command, check for player input
	else if (_state == BATTLE_STATE_NORMAL) {
		// Check for victory or defeat conditions
		if (_NumberCharactersAlive() == 0) {
			ChangeState(BATTLE_STATE_DEFEAT);
		}
		else if (_NumberEnemiesAlive() == 0) {
			ChangeState(BATTLE_STATE_VICTORY);
		}

		// Check whether the last enemy is dying
		if (_last_enemy_dying == false && _NumberValidEnemies() == 0)
		    _last_enemy_dying = true;

		// Holds a pointer to the character to select an action for
		BattleCharacter* character_selection = NULL;

		// The four keys below (up/down/left/right) correspond to each character, from top to bottom. Since the character party
		// does not always have four characters, for all but the first key we have to check that a character exists for the
		// corresponding key. If a character does exist, we then have to check whether or not the player is allowed to select a command
		// for it (characters can only have commands selected during certain states). If command selection is permitted, then we begin
		// the command supervisor.

		if (InputManager->UpPress()) {
			if  (_character_actors.size() >= 1) { // Should always evaluate to true
				character_selection = _character_actors[0];
			}
		}

		else if (InputManager->DownPress()) {
			if  (_character_actors.size() >= 2) {
				character_selection = _character_actors[1];
			}
		}

		else if (InputManager->LeftPress()) {
			if  (_character_actors.size() >= 3) {
				character_selection = _character_actors[2];
			}
		}

		else if (InputManager->RightPress()) {
			if  (_character_actors.size() >= 4) {
				character_selection = _character_actors[3];
			}
		}

		if (!_last_enemy_dying && character_selection != NULL) {
			OpenCommandMenu(character_selection);
		}
	}
	// If the player is selecting a command for a character, the command supervisor has control
	else if (_state == BATTLE_STATE_COMMAND) {
		// If the last enemy is dying, there is no need to process command further
		if (!_last_enemy_dying)
			_command_supervisor->Update();
		else
			ChangeState(BATTLE_STATE_NORMAL);
	}
	// If the battle is in either finish state, the finish supervisor has control
	else if ((_state == BATTLE_STATE_VICTORY) || (_state == BATTLE_STATE_DEFEAT)) {
		_finish_supervisor->Update();
		return;
	}

	// If the battle is running in the "wait" setting, we need to pause the battle whenever any character reaches the
	// command state to allow the player to enter a command for that character before resuming. We also want to make sure
	// that the command menu is open whenever we find a character in the command state. If the command menu is not open, we
	// forcibly open it and make the player choose a command for the character so that the battle may continue.
	if (!_last_enemy_dying && GlobalManager->GetBattleSetting() == GLOBAL_BATTLE_WAIT) {
		for (uint32 i = 0; i < _character_actors.size(); i++) {
			if (_character_actors[i]->GetState() == ACTOR_STATE_COMMAND) {
				if (_state != BATTLE_STATE_COMMAND) {
					OpenCommandMenu(_character_actors[i]);
				}
				return;
			}
		}
	}

	// Process the actor ready queue
	if (!_last_enemy_dying && !_ready_queue.empty()) {
		// Only the acting actor is examined in the ready queue. If this actor is in the READY state,
		// that means it has been waiting for BattleMode to allow it to begin its action and thus
		// we set it to the ACTING state. We do nothing while it is in the ACTING state, allowing the
		// actor to completely finish its action. When the actor enters any other state, it is presumed
		// to be finished with the action or otherwise incapacitated and is removed from the queue.
		BattleActor* acting_actor = _ready_queue.front();
		switch (acting_actor->GetState()) {
			case ACTOR_STATE_READY:
				acting_actor->ChangeState(ACTOR_STATE_ACTING);
				break;
			case ACTOR_STATE_ACTING:
				break;
			default:
				_ready_queue.pop_front();
				break;
		}
	}

	// Update all actors
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		_character_actors[i]->Update();
	}
	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		_enemy_actors[i]->Update();
	}
} // void BattleMode::Update()



void BattleMode::Draw() {
	// Restore correct display metrics
	// Remove me once the particle manager is using the same ones
	VideoManager->SetCoordSys(0.0f, VIDEO_STANDARD_RES_WIDTH, 0.0f, VIDEO_STANDARD_RES_HEIGHT);

	if (_state == BATTLE_STATE_INITIAL || _state == BATTLE_STATE_EXITING) {
		_sequence_supervisor->Draw();
		return;
	}

	_DrawBackgroundGraphics();
	_DrawSprites();
	_DrawForegroundGraphics();

	for (uint32 i = 0; i < _draw_effects_functions.size(); ++i)
		ReadScriptDescriptor::RunScriptObject(_draw_effects_functions[i]);
}

void BattleMode::DrawPostEffects() {
	// Restore correct display metrics
	// Remove me once the particle manager is using the same ones
	VideoManager->SetCoordSys(0.0f, VIDEO_STANDARD_RES_WIDTH, 0.0f, VIDEO_STANDARD_RES_HEIGHT);

	if (_state == BATTLE_STATE_INITIAL || _state == BATTLE_STATE_EXITING) {
		_sequence_supervisor->DrawPostEffects();
		return;
	}

	_DrawGUI();
}

////////////////////////////////////////////////////////////////////////////////
// BattleMode class -- secondary methods
////////////////////////////////////////////////////////////////////////////////

void BattleMode::AddEnemy(GlobalEnemy* new_enemy) {
	// Don't add the enemy if it has an invalid ID or an experience level that is not zero
	if (new_enemy->GetID() == 0) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "attempted to add a new enemy with an invalid id: " << new_enemy->GetID() << endl;
		return;
	}
	if (new_enemy->GetExperienceLevel() != 0) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "attempted to add a new enemy that had already been initialized: " << new_enemy->GetID() << endl;
		return;
	}

	new_enemy->Initialize();
	BattleEnemy* new_battle_enemy = new BattleEnemy(new_enemy);
	_enemy_actors.push_back(new_battle_enemy);
	_enemy_party.push_back(new_battle_enemy);
}



void BattleMode::AddBattleScript(const std::string& filename) {
	if (_state != BATTLE_STATE_INVALID) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "function was called when battle mode was already initialized" << endl;
		return;
	}

	_script_filenames.push_back(filename);
}



void BattleMode::RestartBattle() {
	// Disable potential previous light effects
	VideoManager->DisableFadeEffect();

	// Reset the state of all characters and enemies
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		_character_actors[i]->ResetActor();
	}

	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		_enemy_actors[i]->ResetActor();
	}

	_battle_media.battle_music.Rewind();
	_battle_media.battle_music.Play();

	_last_enemy_dying = false;

	ChangeState(BATTLE_STATE_INITIAL);
}



void BattleMode::FreezeTimers() {
	// Pause character and enemy state timers
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		_character_actors[i]->GetStateTimer().Pause();
	}
	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		_enemy_actors[i]->GetStateTimer().Pause();
	}
}



void BattleMode::UnFreezeTimers() {
	// FIXME: Do not unpause timers for paralyzed actors

	// Unpause character and enemy state timers
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		_character_actors[i]->GetStateTimer().Run();
	}
	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		_enemy_actors[i]->GetStateTimer().Run();
	}
}



void BattleMode::ChangeState(BATTLE_STATE new_state) {
	if (_state == new_state) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "battle was already in the state to change to: " << _state << endl;
		return;
	}

	_state = new_state;
	switch (_state) {
		case BATTLE_STATE_INITIAL:
			break;
		case BATTLE_STATE_NORMAL:
			break;
		case BATTLE_STATE_COMMAND:
			if (_command_supervisor->GetCommandCharacter() == NULL) {
				IF_PRINT_WARNING(BATTLE_DEBUG) << "no character was selected when changing battle to the command state" << endl;
				_state = BATTLE_STATE_NORMAL;
			}
			break;
		case BATTLE_STATE_EVENT:
			// TODO
			break;
		case BATTLE_STATE_VICTORY:
			// Official victory:
			// Cancel all character actions to free possible involved objects
			for (uint32 i = 0; i < _character_actors.size(); ++i) {
				BattleAction *action = _character_actors[i]->GetAction();
				if (action)
					action->Cancel();
			}

			// Remove the items used in battle from inventory.
			_command_supervisor->CommitChangesToInventory();

			_battle_media.victory_music.Play();
			_finish_supervisor->Initialize(true);
			break;
		case BATTLE_STATE_DEFEAT:
			//Apply scene lighting if the battle has finished badly
			GetEffectSupervisor().EnableLightingOverlay(Color(1.0f, 0.0f, 0.0f, 0.4f));

			_battle_media.defeat_music.Play();
			_finish_supervisor->Initialize(false);
			break;
		default:
			IF_PRINT_WARNING(BATTLE_DEBUG) << "changed to invalid battle state: " << _state << endl;
			break;
	}
}



bool BattleMode::OpenCommandMenu(BattleCharacter* character) {
	if (character == NULL) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "function received NULL argument" << endl;
		return false;
	}
	if (_state == BATTLE_STATE_COMMAND) {
		return false;
	}

	if (character->CanSelectCommand() == true) {
		_command_supervisor->Initialize(character);
		ChangeState(BATTLE_STATE_COMMAND);
		return true;
	}

	return false;
}


void BattleMode::NotifyCommandCancel() {
	if (_state != BATTLE_STATE_COMMAND) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "battle was not in command state when function was called" << endl;
		return;
	}
	else if (_command_supervisor->GetCommandCharacter() != NULL) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "command supervisor still had a character selected when function was called" << endl;
		return;
	}

	ChangeState(BATTLE_STATE_NORMAL);
}



void BattleMode::NotifyCharacterCommandComplete(BattleCharacter* character) {
	if (character == NULL) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "function received NULL argument" << endl;
		return;
	}

	// Update the action text to reflect the action and target now set for the character
	character->ChangeActionText();

	// If the character was in the command state when it had its command set, the actor needs to move on the the warmup state to prepare to
	// execute the command. Otherwise if the character was in any other state (likely the idle state), the character should remain in that state.
	if (character->GetState() == ACTOR_STATE_COMMAND) {
		character->ChangeState(ACTOR_STATE_WARM_UP);
	}

	ChangeState(BATTLE_STATE_NORMAL);
}



void BattleMode::NotifyActorReady(BattleActor* actor) {
	for (list<BattleActor*>::iterator i = _ready_queue.begin(); i != _ready_queue.end(); i++) {
		if (actor == (*i)) {
			IF_PRINT_WARNING(BATTLE_DEBUG) << "actor was already present in the ready queue" << endl;
			return;
		}
	}

	_ready_queue.push_back(actor);
}



void BattleMode::NotifyActorDeath(BattleActor* actor) {
	if (actor == NULL) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "function received NULL argument" << endl;
		return;
	}

	// Remove the actor from the ready queue if it is there
	_ready_queue.remove(actor);

	// Notify the command supervisor about the death event if it is active
	if (_state == BATTLE_STATE_COMMAND) {
		_command_supervisor->NotifyActorDeath(actor);

		// If the actor who died was the character that the player was selecting a command for, this will cause the
		// command supervisor will return to the invalid state.
		if (_command_supervisor->GetState() == COMMAND_STATE_INVALID)
			ChangeState(BATTLE_STATE_NORMAL);
	}

	// Determine if the battle should proceed to the victory or defeat state
	if (IsBattleFinished())
		IF_PRINT_WARNING(BATTLE_DEBUG) << "actor death occurred after battle was finished" << endl;
}

////////////////////////////////////////////////////////////////////////////////
// BattleMode class -- private methods
////////////////////////////////////////////////////////////////////////////////

void BattleMode::_Initialize() {
	// Unset a possible last enemy dying sequence.
	_last_enemy_dying = false;

	// (1): Construct all character battle actors from the active party, as well as the menus that populate the command supervisor
	GlobalParty* active_party = GlobalManager->GetActiveParty();
	if (active_party->GetPartySize() == 0) {
		IF_PRINT_WARNING(BATTLE_DEBUG) << "no characters in the active party, exiting battle" << endl;
		ModeManager->Pop();
		return;
	}

	for (uint32 i = 0; i < active_party->GetPartySize(); i++) {
		BattleCharacter* new_actor = new BattleCharacter(dynamic_cast<GlobalCharacter*>(active_party->GetActorAtIndex(i)));
		_character_actors.push_back(new_actor);
		_character_party.push_back(new_actor);
	}
	_command_supervisor->ConstructMenus();

	// (2): Determine the origin position for all characters and enemies
	_DetermineActorLocations();

	// (3): Find the actor with the highext agility rating
	uint32 highest_agility = 0;
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		if (_character_actors[i]->GetAgility() > highest_agility)
			highest_agility = _character_actors[i]->GetAgility();
	}
	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		if (_enemy_actors[i]->GetAgility() > highest_agility)
			highest_agility = _enemy_actors[i]->GetAgility();
	}

	// Andy: Once every game loop, the SystemManager's timers are updated
	// However, in between calls, battle mode is constructed. As part
	// of battle mode's construction, each actor is given a wait timer
	// that is triggered on initialization. But the moving of the stamina
	// portrait uses the update time from SystemManager.  Therefore, the
	// amount of time since SystemManager last updated is greater than
	// the amount of time that has expired on the actors' wait timers
	// during the first orund of battle mode.  This gives the portrait an
	// extra boost, so once the wait time expires for an actor, his portrait
	// is past the designated stopping point

	// <--      time       -->
	// A----------X-----------B
	// If the SystemManager has its timers updated at A and B, and battle mode is
	// constructed and initialized at X, you can see the amount of time between
	// X and B (how much time passed on the wait timers in round 1) is significantly
	// smaller than the time between A and B.  Hence the extra boost to the stamina
	// portrait's location

	// FIX ME This will not work in the future (i.e. paralysis)...realized this
	// after writing all the above crap
	// CD: Had to move this to before timers are initalized, otherwise this call will give
	// our timers a little extra nudge with regards to time elapsed, thus making the portraits
	// stop before they reach they yellow/orange line
	// TODO: This should be fixed once battles have a little smoother start (characters run in from
	// off screen to their positions, and stamina icons do not move until they are ready in their
	// battle positions). Once that feature is available, remove this call.
	SystemManager->UpdateTimers();

	// (4): Adjust each actor's idle state time based on their agility proportion to the fastest actor
	// If an actor's agility is half that of the actor with the highest agility, then they will have an
	// idle state time that is twice that of the slowest actor.
	float proportion;
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		proportion = static_cast<float>(highest_agility) / static_cast<float>(_character_actors[i]->GetAgility());
		_character_actors[i]->SetIdleStateTime(static_cast<uint32>(MIN_IDLE_WAIT_TIME * proportion));
		_character_actors[i]->ChangeState(ACTOR_STATE_IDLE);
	}
	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		proportion = static_cast<float>(highest_agility) / static_cast<float>(_enemy_actors[i]->GetAgility());
		_enemy_actors[i]->SetIdleStateTime(static_cast<uint32>(MIN_IDLE_WAIT_TIME * proportion));
		_enemy_actors[i]->ChangeState(ACTOR_STATE_IDLE);
	}

	// (5): Randomize each actor's initial idle state progress to be somewhere in the lower half of their total
	// idle state time. This is performed so that every battle doesn't start will all stamina icons piled on top
	// of one another at the bottom of the stamina bar
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		uint32 max_init_timer = _character_actors[i]->GetIdleStateTime() / 2;
		_character_actors[i]->GetStateTimer().Update(RandomBoundedInteger(0, max_init_timer));

	}
	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		uint32 max_init_timer = _enemy_actors[i]->GetIdleStateTime() / 2;
		_enemy_actors[i]->GetStateTimer().Update(RandomBoundedInteger(0, max_init_timer));
	}

	// Open every possible battle script files registered and process them.
	for (uint32 i = 0; i < _script_filenames.size(); ++i) {
		ReadScriptDescriptor battle_script;
		if (!battle_script.OpenFile(_script_filenames[i]))
			continue;

		if (battle_script.OpenTablespace().empty()) {
			PRINT_ERROR << "The battle script file: " << _script_filenames[i] << "has not set a correct namespace" << endl;
			continue;
		}

		_update_functions.push_back(battle_script.ReadFunctionPointer("Update"));
		_draw_background_functions.push_back(battle_script.ReadFunctionPointer("DrawBackground"));
		_draw_foreground_functions.push_back(battle_script.ReadFunctionPointer("DrawForeground"));
		_draw_effects_functions.push_back(battle_script.ReadFunctionPointer("DrawEffects"));

		// Trigger the Initialize functions in the loading order.
		ScriptObject init_function = battle_script.ReadFunctionPointer("Initialize");
		if (init_function.is_valid())
			ScriptCallFunction<void>(init_function, this);

		battle_script.CloseTable(); // The tablespace
		battle_script.CloseFile();
	}

	ChangeState(BATTLE_STATE_INITIAL);
} // void BattleMode::_Initialize()



void BattleMode::_DetermineActorLocations() {
	// Temporary static positions for enemies
	const float TEMP_ENEMY_LOCATIONS[][2] = {
		{ 515.0f, 768.0f - 600.0f }, // 768.0f - because of reverse Y-coordinate system
		{ 494.0f, 768.0f - 450.0f },
		{ 560.0f, 768.0f - 550.0f },
		{ 580.0f, 768.0f - 630.0f },
		{ 675.0f, 768.0f - 390.0f },
		{ 655.0f, 768.0f - 494.0f },
		{ 793.0f, 768.0f - 505.0f },
		{ 730.0f, 768.0f - 600.0f }
	};

	float position_x, position_y;

	// Determine the position of the first character in the party, who will be drawn at the top
	switch (_character_actors.size()) {
		case 1:
			position_x = 80.0f;
			position_y = 288.0f;
			break;
		case 2:
			position_x = 118.0f;
			position_y = 343.0f;
			break;
		case 3:
			position_x = 122.0f;
			position_y = 393.0f;
			break;
		case 4:
		default:
			position_x = 160.0f;
			position_y = 448.0f;
			break;
	}

	// Set all characters in their proper positions
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		_character_actors[i]->SetXOrigin(position_x);
		_character_actors[i]->SetYOrigin(position_y);
		_character_actors[i]->SetXLocation(position_x);
		_character_actors[i]->SetYLocation(position_y);
		position_x -= 32.0f;
		position_y -= 105.0f;
	}

	// TEMP: assign static locations to enemies
	uint32 temp_pos = 0;
	for (uint32 i = 0; i < _enemy_actors.size(); i++, temp_pos++) {
		position_x = TEMP_ENEMY_LOCATIONS[temp_pos][0];
		position_y = TEMP_ENEMY_LOCATIONS[temp_pos][1];
		_enemy_actors[i]->SetXOrigin(position_x);
		_enemy_actors[i]->SetYOrigin(position_y);
		_enemy_actors[i]->SetXLocation(position_x);
		_enemy_actors[i]->SetYLocation(position_y);
	}
} // void BattleMode::_DetermineActorLocations()



uint32 BattleMode::_NumberEnemiesAlive() const {
	uint32 enemy_count = 0;
	for (uint32 i = 0; i < _enemy_actors.size(); ++i) {
		if (_enemy_actors[i]->IsAlive()) {
			++enemy_count;
		}
	}
	return enemy_count;
}


uint32 BattleMode::_NumberValidEnemies() const {
	uint32 enemy_count = 0;
	for (uint32 i = 0; i < _enemy_actors.size(); ++i) {
		if (_enemy_actors[i]->IsValid()) {
			++enemy_count;
		}
	}
	return enemy_count;
}


uint32 BattleMode::_NumberCharactersAlive() const {
	uint32 character_count = 0;
	for (uint32 i = 0; i < _character_actors.size(); ++i) {
		if (_character_actors[i]->IsAlive()) {
			++character_count;
		}
	}
	return character_count;
}


void BattleMode::_DrawBackgroundGraphics() {
	VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, VIDEO_NO_BLEND, 0);
	VideoManager->Move(0.0f, 0.0f);
	_battle_media.background_image.Draw();

	VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_TOP, VIDEO_BLEND, 0);
	VideoManager->SetCoordSys(0.0f, VIDEO_STANDARD_RES_WIDTH, 0.0f, VIDEO_STANDARD_RES_HEIGHT);

	// Handles custom scripted draw before sprites
	for (uint32 i = 0; i < _draw_background_functions.size(); ++i)
		ReadScriptDescriptor::RunScriptObject(_draw_background_functions[i]);
}


void BattleMode::_DrawForegroundGraphics() {
	VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_TOP, VIDEO_BLEND, 0);
	VideoManager->SetCoordSys(0.0f, VIDEO_STANDARD_RES_WIDTH, 0.0f, VIDEO_STANDARD_RES_HEIGHT);

	for (uint32 i = 0; i < _draw_foreground_functions.size(); ++i)
	    ReadScriptDescriptor::RunScriptObject(_draw_foreground_functions[i]);
}


void BattleMode::_DrawSprites() {
	// Booleans used to determine whether or not the actor selector and attack point selector graphics should be drawn
	bool draw_actor_selection = false;
	bool draw_point_selection = false;

	BattleTarget target = _command_supervisor->GetSelectedTarget(); // The target that the player has selected
	BattleActor* actor_target = target.GetActor(); // A pointer to an actor being targetted (value may be NULL if target is party)

	// Determine if selector graphics should be drawn
	if ((_state == BATTLE_STATE_COMMAND)
		&& ((_command_supervisor->GetState() == COMMAND_STATE_ACTOR) || (_command_supervisor->GetState() == COMMAND_STATE_POINT))) {
		draw_actor_selection = true;
		if ((_command_supervisor->GetState() == COMMAND_STATE_POINT) && (IsTargetPoint(target.GetType()) == true))
			draw_point_selection = true;
	}

	// Draw the actor selector graphic
	if (draw_actor_selection == true) {
		VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, VIDEO_BLEND, 0);
		if (actor_target != NULL) {
			VideoManager->Move(actor_target->GetXLocation(), actor_target->GetYLocation());
			VideoManager->MoveRelative(0.0f, -20.0f);
			_battle_media.actor_selection_image.Draw();
		}
		else if (IsTargetParty(target.GetType()) == true) {
			deque<BattleActor*>& party_target = *(target.GetParty());
			for (uint32 i = 0; i < party_target.size(); i++) {
				VideoManager->Move(party_target[i]->GetXLocation(),  party_target[i]->GetYLocation());
				VideoManager->MoveRelative(0.0f, -20.0f);
				_battle_media.actor_selection_image.Draw();
			}
			actor_target = NULL;
			// TODO: add support for drawing graphic under multiple actors if the target is a party
		}
		// Else this target is invalid so don't draw anything
	}

	// TODO: Draw sprites in order based on their x and y coordinates on the screen (bottom to top, then left to right)
	// Right now they are drawn randomly, which would look bad/inconsistent if the sprites overlapped during their actions

	// Draw all character sprites
	VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, VIDEO_BLEND, 0);
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		_character_actors[i]->DrawSprite();
	}

	// Draw all enemy sprites
	for (uint32 i = 0; i < _enemy_actors.size(); i++) {
		_enemy_actors[i]->DrawSprite();
	}

	// Draw the attack point selector graphic
	if (draw_point_selection == true) {
		uint32 point = target.GetPoint();

		VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_CENTER, VIDEO_BLEND, 0);
		VideoManager->Move(actor_target->GetXLocation(), actor_target->GetYLocation());
		VideoManager->MoveRelative(actor_target->GetAttackPoint(point)->GetXPosition(), actor_target->GetAttackPoint(point)->GetYPosition());
		_battle_media.attack_point_indicator.Draw();
	}
} // void BattleMode::_DrawSprites()



void BattleMode::_DrawGUI() {
	_DrawBottomMenu();
	_DrawStaminaBar();

	// Don't draw battle actor indicators at battle ends
	if (_state != BATTLE_STATE_VICTORY && _state != BATTLE_STATE_DEFEAT)
		_DrawIndicators();

	if (_command_supervisor->GetState() != COMMAND_STATE_INVALID) {
		if ((_dialogue_supervisor->IsDialogueActive() == true) && (_dialogue_supervisor->GetCurrentDialogue()->IsHaltBattleAction() == true)) {
			// Do not draw the command selection GUI if a dialogue is active that halts the battle action
		}
		else {
			_command_supervisor->Draw();
		}
	}
	if (_dialogue_supervisor->IsDialogueActive() == true) {
		_dialogue_supervisor->Draw();
	}
	if ((_state == BATTLE_STATE_VICTORY || _state == BATTLE_STATE_DEFEAT)) {
		_finish_supervisor->Draw();
	}
}



void BattleMode::_DrawBottomMenu() {
	// Draw the static image for the lower menu
	VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, VIDEO_BLEND, 0);
	VideoManager->Move(0.0f, 0.0f);
	_battle_media.bottom_menu_image.Draw();

	// Draw the swap icon and any swap cards
	VideoManager->Move(6.0f, 16.0f);
	_battle_media.swap_icon.Draw(Color::gray);
	VideoManager->Move(6.0f, 68.0f);
	for (uint8 i = 0; i < _current_number_swaps; i++) {
		_battle_media.swap_card.Draw();
		VideoManager->MoveRelative(4.0f, -4.0f);
	}

	// If the player is selecting a command for a particular character, draw that character's portrait
	if (_command_supervisor->GetCommandCharacter() != NULL)
		_command_supervisor->GetCommandCharacter()->DrawPortrait();

	// Draw the highlight images for the character that a command is being selected for (if any) and/or any characters
	// that are in the "command" state. The latter indicates that these characters needs a command selected as soon as possible
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		if (_character_actors[i] == _command_supervisor->GetCommandCharacter()) {
			VideoManager->Move(148.0f, 85.0f - (25.0f * i));
			_battle_media.character_selected_highlight.Draw();
		}
		else if (_character_actors[i]->GetState() == ACTOR_STATE_COMMAND) {
			VideoManager->Move(148.0f, 85.0f - (25.0f * i));
			_battle_media.character_command_highlight.Draw();
		}
	}

	// Draw the status information of all character actors
	for (uint32 i = 0; i < _character_actors.size(); i++) {
		_character_actors[i]->DrawStatus(i);
	}
}



void BattleMode::_DrawStaminaBar() {
	// Used to determine whether or not an icon selector graphic needs to be drawn
	bool draw_icon_selection = false;
	// If true, an entire party of actors is selected
	bool is_party_selected = false;
	// If true, the selected party is the enemy party
	bool is_party_enemy = false;

	BattleActor* selected_actor = NULL; // A pointer to the selected actor

	// Determine whether the selector graphics should be drawn
	if ((_state == BATTLE_STATE_COMMAND)
		&& ((_command_supervisor->GetState() == COMMAND_STATE_ACTOR) || (_command_supervisor->GetState() == COMMAND_STATE_POINT))) {
		BattleTarget target = _command_supervisor->GetSelectedTarget();

		draw_icon_selection = true;
		selected_actor = target.GetActor(); // Will remain NULL if the target type is a party

		if (target.GetType() == GLOBAL_TARGET_ALL_ALLIES) {
			is_party_selected = true;
			is_party_enemy = false;
		}
		else if (target.GetType() == GLOBAL_TARGET_ALL_FOES) {
			is_party_selected = true;
			is_party_enemy = true;
		}
	}

	// TODO: sort the draw positions

	// Draw the stamina bar
	VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, 0);
	VideoManager->Move(STAMINA_BAR_POSITION_X, STAMINA_BAR_POSITION_Y); // 1010
	_battle_media.stamina_meter.Draw();

	// Draw all stamina icons in order along with the selector graphic
	VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_CENTER, 0);

    for (uint32 i = 0; i < _character_actors.size(); ++i) {
        if (!_character_actors[i]->IsAlive())
            continue;

        _character_actors[i]->DrawStaminaIcon();

        if (!draw_icon_selection)
            continue;

        // Draw selections
        if ((is_party_selected && !is_party_enemy) || _character_actors[i] == selected_actor)
            _battle_media.stamina_icon_selected.Draw();
    }

    for (uint32 i = 0; i < _enemy_actors.size(); ++i) {
        if (!_enemy_actors[i]->IsAlive())
            continue;

        _enemy_actors[i]->DrawStaminaIcon();

        if (!draw_icon_selection)
            continue;

        // Draw selections
        if ((is_party_selected && is_party_enemy) || _enemy_actors[i] == selected_actor)
            _battle_media.stamina_icon_selected.Draw();
    }
} // void BattleMode::_DrawStaminaBar()



void BattleMode::_DrawIndicators() {
	// Draw all character sprites
	for (uint32 i = 0; i < _character_actors.size(); ++i) {
		_character_actors[i]->DrawIndicators();
	}

	// Draw all enemy sprites
	for (uint32 i = 0; i < _enemy_actors.size(); ++i) {
		_enemy_actors[i]->DrawIndicators();
	}
}


// Available encounter sounds
static std::string encounter_sound_filenames[] = {
	"snd/battle_encounter_01.ogg",
	"snd/battle_encounter_02.ogg",
	"snd/battle_encounter_03.ogg" };

TransitionToBattleMode::TransitionToBattleMode(BattleMode *BM):
	_position(0.0f),
	_BM(BM) {

	_screen_capture = VideoManager->CaptureScreen();
	_screen_capture.SetDimensions(1024.0f, 769.0f);
}

void TransitionToBattleMode::Update() {
	_transition_timer.Update();

	_position += _transition_timer.PercentComplete();

	if (_BM && _transition_timer.IsFinished()) {
		ModeManager->Pop();
		ModeManager->Push(_BM, true, true);
		_BM = NULL;
	}
}

void TransitionToBattleMode::Draw() {
	// Draw the battle transition effect
	VideoManager->SetStandardCoordSys();
	VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_TOP, VIDEO_BLEND, 0);
	VideoManager->Move(0.0f, 0.0f);
	_screen_capture.Draw();
	VideoManager->Move(_position, _position);
	_screen_capture.Draw(Color(1.0f, 1.0f, 1.0f, 0.3f));
	VideoManager->Move(-_position, _position);
	_screen_capture.Draw(Color(1.0f, 1.0f, 1.0f, 0.3f));
	VideoManager->Move(-_position, -_position);
	_screen_capture.Draw(Color(1.0f, 1.0f, 1.0f, 0.3f));
	VideoManager->Move(_position, -_position);
	_screen_capture.Draw(Color(1.0f, 1.0f, 1.0f, 0.3f));
}

void TransitionToBattleMode::Reset() {

	_position = 0.0f;
	_transition_timer.Initialize(1500, SYSTEM_TIMER_NO_LOOPS);
	_transition_timer.Run();

	// Stop the map music
	AudioManager->StopAllMusic();

	// Play a random encounter sound
	uint32 file_id = hoa_utils::RandomBoundedInteger(0, 2);
	hoa_audio::AudioManager->PlaySound(encounter_sound_filenames[file_id]);
}

} // namespace hoa_battle
