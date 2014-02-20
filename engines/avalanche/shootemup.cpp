/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
* This code is based on the original source code of Lord Avalot d'Argent version 1.3.
* Copyright (c) 1994-1995 Mike, Mark and Thomas Thurman.
*/

#include "avalanche/avalanche.h"
#include "avalanche/shootemup.h"

#include "common/random.h"

namespace Avalanche {

const byte ShootEmUp::kStocks = 27;
const byte ShootEmUp::kAvvyShoots = 86;
const byte ShootEmUp::kFacingRight = 87;
const byte ShootEmUp::kFacingLeft = 93;
const long int ShootEmUp::kFlag = -20047;
const byte ShootEmUp::kFrameDelayMax = 2;
const byte ShootEmUp::kAvvyY = 150;
const byte ShootEmUp::kShooting[7] = { 87, 80, 81, 82, 81, 80, 87 };

ShootEmUp::ShootEmUp(AvalancheEngine *vm) {
	_vm = vm;

	_time = 0;
	for (int i = 0; i < 7; i++)
		_stockStatus[i] = 0;
	for (int i = 0; i < 99; i++) {
		_sprites[i]._ix = 0;
		_sprites[i]._iy = 0;
		_sprites[i]._x = 0;
		_sprites[i]._y = 0;
		_sprites[i]._p = 0;
		_sprites[i]._timeout = 0;
		_sprites[i]._cameo = false;
		_sprites[i]._cameoFrame = 0;
		_sprites[i]._missile = false;
		_sprites[i]._wipe = false;
	}
	_rectNum = 0;
	_avvyWas = 0;
	_avvyPos = 0;
	_avvyAnim = 0;
	_avvyFacing = 0;
	_altWasPressedBefore = false;
	_throwNext = 0;
	_firing = false;
	for (int i = 0; i < 4; i++) {
		_running[i]._x = 0;
		_running[i]._y = 0;
		_running[i]._frame = 0;
		_running[i]._tooHigh = 0;
		_running[i]._lowest = 0;
		_running[i]._ix = 0;
		_running[i]._iy = 0;
		_running[i]._frameDelay = 0;
	}
	for (int i = 0; i < 7; i++)
		_hasEscaped[i] = false;
	_count321 = 0;
	_howManyHaveEscaped = 0;
	_escapeCount = 0;
	_escaping = false;
	_timeThisSecond = 0;
	_cp = false;
	_wasFacing = 0;
	_score = 0;
	_escapeStock = 0;
	_gotOut = false;
}

void ShootEmUp::run() {
	CursorMan.showMouse(false);
	_vm->_graphics->saveScreen();
	_vm->fadeOut();
	_vm->_graphics->seuDrawTitle();
	_vm->fadeIn();

	_vm->_graphics->seuLoad();

	// Should we show the instructions?
	while (!_vm->shouldQuit()) {
		Common::Event event;
		_vm->getEvent(event);
		if (event.type == Common::EVENT_KEYDOWN) {
			if ((event.kbd.keycode == Common::KEYCODE_i) || (event.kbd.keycode == Common::KEYCODE_F1))
				instructions();
			break; // We don't show the instructions and simply go on with the minigame if not I or F1 was pressed.
		}
	}

	setup();
	
	do {
		blankIt();
		hitPeople();
		plotThem();
		moveThem();
		moveAvvy();
		bumpFolk();
		peopleRunning();
		animate();
		escapeCheck();
		collisionCheck();
		updateTime();
		check321();
		readKbd();

		_cp = !_cp;

		_vm->_graphics->refreshScreen();
	} while (_time != 0);

	_vm->fadeOut();
	_vm->_graphics->restoreScreen();
	_vm->_graphics->removeBackup();
	_vm->fadeIn();
	CursorMan.showMouse(true);
}

bool ShootEmUp::overlap(uint16 a1x, uint16 a1y, uint16 a2x, uint16 a2y, uint16 b1x, uint16 b1y, uint16 b2x, uint16 b2y) {
	// By De Morgan's law:
	return (a2x >= b1x) && (b2x >= a1x) && (a2y >= b1y) && (b2y >= a1y);
}

byte ShootEmUp::getStockNumber(byte index) {
	while (_hasEscaped[index]) {
		index++;
		if (index == 7)
			index = 0;
	}
	return index;
}

void ShootEmUp::blankIt() {
	for (int i = 0; i < _rectNum; i++)
		_vm->_graphics->drawFilledRectangle(_rectangles[i], kColorBlack);
	_rectNum = 0;
}

void ShootEmUp::moveThem() {
	for (int i = 0; i < 99; i++) {
		if (_sprites[i]._x != kFlag) {
			_sprites[i]._x += _sprites[i]._ix;
			_sprites[i]._y += _sprites[i]._iy;
		}
	}
}

void ShootEmUp::blank(Common::Rect rect) {
	_rectangles[_rectNum++] = rect;
}

void ShootEmUp::plotThem() {
	for (int i = 0; i < 99; i++) {
		if (_sprites[i]._x != kFlag) {
			if (_sprites[i]._cameo) {
				_vm->_graphics->seuDrawCameo(_sprites[i]._x, _sprites[i]._y, _sprites[i]._p, _sprites[i]._cameoFrame);
				if (!_cp) {
					_sprites[i]._cameoFrame += 2;
					_sprites[i]._p += 2;
				}
			} else
				_vm->_graphics->seuDrawPicture(_sprites[i]._x, _sprites[i]._y, _sprites[i]._p);

			if (_sprites[i]._wipe)
				blank(Common::Rect(_sprites[i]._x, _sprites[i]._y, _sprites[i]._x + _rectangles[i].width(), _sprites[i]._y + _rectangles[i].height()));

			if (_sprites[i]._timeout > 0) {
				_sprites[i]._timeout--;
				if (_sprites[i]._timeout == 0)
					_sprites[i]._y = kFlag;
			}
		}
	}
}

void ShootEmUp::define(int16 x, int16 y, byte p, int8 ix, int8 iy, int16 time, bool isAMissile, bool doWeWipe) {
	for (int i = 0; i < 99; i++) {
		if (_sprites[i]._x == kFlag) {
			_sprites[i]._x = x;
			_sprites[i]._y = y;
			_sprites[i]._p = p;
			_sprites[i]._ix = ix;
			_sprites[i]._iy = iy;
			_sprites[i]._timeout = time;
			_sprites[i]._cameo = false;
			_sprites[i]._missile = isAMissile;
			_sprites[i]._wipe = doWeWipe;
		}
	}
}

void ShootEmUp::defineCameo(int16 xx, int16 yy, byte pp, int16 time) {
	warning("STUB: ShootEmUp::defineCameo()");
}

void ShootEmUp::showStock(byte index) {
	if (_escaping && (index == _escapeStock)) {
		_vm->_graphics->seuDrawPicture(index * 90 + 20, 30, kStocks + 2);
		return;
	}

	if (_stockStatus[index] > 5)
		return;

	_vm->_graphics->seuDrawPicture(index * 90 + 20, 30, kStocks + _stockStatus[index]);
	_stockStatus[index] = 1 - _stockStatus[index];
}

void ShootEmUp::drawNumber(int number, int size, int x) {
	for (int i = 0; i < size - 1; i++) {
		int divisor = 10;
		for (int j = 0; j < size - 2 - i; j++)
			divisor *= 10;
		char value = number / divisor;
		_vm->_graphics->seuDrawPicture(x + i * 10, 0, value);
		number -= value * divisor;
	}
	_vm->_graphics->seuDrawPicture(x + (size - 1) * 10, 0, number % 10);
}

void ShootEmUp::showScore() {
	drawNumber(_score, 5, 40);
}

void ShootEmUp::showTime() {
	drawNumber(_time, 3, 140);
}

void ShootEmUp::gain(int8 howMuch) {
	if ((_score + howMuch) == 0) // howMuch can be negative!
		_score = 0;
	else
		_score += howMuch;

	showScore();
}

void ShootEmUp::newEscape() {
	_escapeCount = _vm->_rnd->getRandomNumber(17) * 20;
	_escaping = false;
}

void ShootEmUp::nextPage() {
	_vm->_graphics->drawNormalText("Press a key for next page >", _vm->_font, 8, 400, 190, kColorWhite);
	_vm->_graphics->refreshScreen();

	while (!_vm->shouldQuit()) {
		Common::Event event;
		_vm->getEvent(event);
		if (event.type == Common::EVENT_KEYDOWN) {
			break;
		}
	}

	_vm->_graphics->drawFilledRectangle(Common::Rect(0, 0, 639, 199), kColorBlack);
}

void ShootEmUp::instructions() {
	_vm->_graphics->blackOutScreen();
	_vm->_graphics->seuDrawPicture(25, 25, kFacingRight);
	_vm->_graphics->drawNormalText("< Avvy, our hero, needs your help - you must move him around.", _vm->_font, 8, 60, 35, kColorWhite);
	_vm->_graphics->drawNormalText("(He''s too terrified to move himself!)", _vm->_font, 8, 80, 45, kColorWhite);
	_vm->_graphics->drawNormalText("Your task is to prevent the people in the stocks from escaping", _vm->_font, 8, 0, 75, kColorWhite);
	_vm->_graphics->drawNormalText("by pelting them with rotten fruit, eggs and bread. The keys are:", _vm->_font, 8, 0, 85, kColorWhite);
	_vm->_graphics->drawNormalText("LEFT SHIFT", _vm->_font, 8, 80, 115, kColorWhite);
	_vm->_graphics->drawNormalText("Move left.", _vm->_font, 8, 200, 115, kColorWhite);
	_vm->_graphics->drawNormalText("RIGHT SHIFT", _vm->_font, 8, 72, 135, kColorWhite);
	_vm->_graphics->drawNormalText("Move right.", _vm->_font, 8, 200, 135, kColorWhite);
	_vm->_graphics->drawNormalText("ALT", _vm->_font, 8, 136, 155, kColorWhite);
	_vm->_graphics->drawNormalText("Throw something.", _vm->_font, 8, 200, 155, kColorWhite);

	nextPage();

	_vm->_graphics->seuDrawPicture(25, 35, kStocks);
	_vm->_graphics->drawNormalText("This man is in the stocks. Your job is to stop him getting out.", _vm->_font, 8, 80, 35, kColorWhite);
	_vm->_graphics->drawNormalText("UNFORTUNATELY... the locks on the stocks are loose, and every", _vm->_font, 8, 88, 45, kColorWhite);
	_vm->_graphics->drawNormalText("so often, someone will discover this and try to get out.", _vm->_font, 8, 88, 55, kColorWhite);
	_vm->_graphics->seuDrawPicture(25, 85, kStocks + 2);
	_vm->_graphics->drawNormalText("< Someone who has found a way out!", _vm->_font, 8, 80, 85, kColorWhite);
	_vm->_graphics->drawNormalText("You MUST IMMEDIATELY hit people smiling like this, or they", _vm->_font, 8, 88, 95, kColorWhite);
	_vm->_graphics->drawNormalText("will disappear and lose you points.", _vm->_font, 8, 88, 105, kColorWhite);
	_vm->_graphics->seuDrawPicture(25, 125, kStocks + 5);
	_vm->_graphics->seuDrawPicture(25, 155, kStocks + 4);
	_vm->_graphics->drawNormalText("< Oh dear!", _vm->_font, 8, 80, 125, kColorWhite);

	nextPage();

	_vm->_graphics->drawNormalText("Your task is made harder by:", _vm->_font, 8, 0, 35, kColorWhite);
	_vm->_graphics->seuDrawPicture(25, 55, 48);
	_vm->_graphics->drawNormalText("< Yokels. These people will run in front of you. If you hit", _vm->_font, 8, 60, 55, kColorWhite);
	_vm->_graphics->drawNormalText("them, you will lose MORE points than you get hitting people", _vm->_font, 8, 68, 65, kColorWhite);
	_vm->_graphics->drawNormalText("in the stocks. So BEWARE!", _vm->_font, 8, 68, 75, kColorWhite);
	_vm->_graphics->drawNormalText("Good luck with the game!", _vm->_font, 8, 80, 125, kColorWhite);

	nextPage();
}

void ShootEmUp::setup() {
	_score = 0;
	_time = 120;

	for (int i = 0; i < 7; i++) {
		_stockStatus[i] = _vm->_rnd->getRandomNumber(1);
		showStock(i);
	}

	_cp = true;

	_avvyWas = 320;
	_avvyPos = 320;
	_avvyAnim = 1;
	_avvyFacing = kFacingLeft;

	_altWasPressedBefore = false;
	_throwNext = 74;
	_firing = false;

	for (int i = 0; i < 4; i++)
		_running[i]._x = kFlag;

	newEscape();

	_count321 = 255; // Counting down.

	_vm->_graphics->blackOutScreen();

	// Set up status line:
	_vm->_graphics->seuDrawPicture(0, 0, 16); // Score:
	showScore(); // Value of score (00000 here).
	_vm->_graphics->seuDrawPicture(110, 0, 19); // Time:
	showTime(); // Value of time.

	_vm->_graphics->refreshScreen();

	// From the original core cycle:
	initRunner(20, 70, 48, 54, _vm->_rnd->getRandomNumber(4) + 1, _vm->_rnd->getRandomNumber(3) - 2);
	initRunner(600, 70, 48, 54, _vm->_rnd->getRandomNumber(4) + 1, _vm->_rnd->getRandomNumber(3) - 2);
	initRunner(600, 100, 61, 67, (-(int8)_vm->_rnd->getRandomNumber(4)) + 1, _vm->_rnd->getRandomNumber(3) - 2);
	initRunner(20, 100, 61, 67, (-(int8)_vm->_rnd->getRandomNumber(4)) + 1, _vm->_rnd->getRandomNumber(3) - 2);
}

void ShootEmUp::initRunner(int16 x, int16 y, byte f1, byte f2, int8 ix, int8 iy) {
	for (int i = 0; i < 4; i++) {
		if (_running[i]._x == kFlag) {
			_running[i]._x = x;
			_running[i]._y = y;
			_running[i]._frame = f1;
			_running[i]._tooHigh = f2;
			_running[i]._lowest = f1;
			_running[i]._ix = ix;
			_running[i]._iy = iy;
			if ((ix = 0) && (iy = 0))
				_running[i]._ix = 2; // To stop them running on the spot!
			_running[i]._frameDelay = kFrameDelayMax;
			return;
		}
	}
}

void ShootEmUp::moveAvvy() {
	if (_avvyWas < _avvyPos)
		_avvyFacing = kFacingRight;
	else if (_avvyWas > _avvyPos)
		_avvyFacing = kFacingLeft;

	if (!_firing) {
		if (_avvyWas == _avvyPos)
			_avvyAnim = 1;
		else {
			_avvyAnim++;
			if (_avvyAnim == 6)
				_avvyAnim = 0;
		}
	}

	if (_avvyFacing == kAvvyShoots)
		define(_avvyPos, kAvvyY, kShooting[_avvyAnim], 0, 0, 1, false, true);
	else
		define(_avvyPos, kAvvyY, _avvyAnim + _avvyFacing, 0, 0, 1, false, true);

	_avvyWas = _avvyPos;

	if (_avvyFacing == kAvvyShoots) {
		if (_avvyAnim == 6) {
			_avvyFacing = _wasFacing;
			_avvyAnim = 0;
			_firing = false;
		} else
			_avvyAnim++;
	}
}

void ShootEmUp::readKbd() {
	warning("STUB: ShootEmUp::readKbd()");
}

void ShootEmUp::animate() {
	if (_vm->_rnd->getRandomNumber(9) == 1)
		showStock(getStockNumber(_vm->_rnd->getRandomNumber(5)));
	for (int i = 0; i < 7; i++) {
		if (_stockStatus[i] > 5) {
			_stockStatus[i]--;
			if (_stockStatus[i] == 8) {
				_stockStatus[i] = 0;
				showStock(i);
			}
		}
	}
}

void ShootEmUp::collisionCheck() {
	warning("STUB: ShootEmUp::collisionCheck()");
}

void ShootEmUp::turnAround(byte who, bool randomX) {
	if (randomX) {
		int8 ix = (_vm->_rnd->getRandomNumber(4) + 1);
		if (_running[who]._ix > 0)
			_running[who]._ix = -(ix);
		else
			_running[who]._ix = ix;
	} else
		_running[who]._ix = -(_running[who]._ix);

	_running[who]._iy = -(_running[who]._iy);
}

void ShootEmUp::bumpFolk() {
	for (int i = 0; i < 4; i++) {
		if (_running[i]._x != kFlag) {
			for (int j = i + 1; j < 4; j++) {
				bool overlaps = overlap(_running[i]._x, _running[i]._y, _running[i]._x + 17, _running[i]._y + 24,
										_running[j]._x, _running[j]._y, _running[j]._x + 17, _running[j]._y + 24);
				if ((_running[i]._x != kFlag) && overlaps) {
					turnAround(i, false); // Opp. directions.
					turnAround(j, false);
				}
			}
		}
	}
}

void ShootEmUp::peopleRunning() {
	if (_count321 != 0)
		return;

	for (int i = 0; i < 4; i++) {
		if (_running[i]._x != kFlag) {
			if (((_running[i]._y + _running[i]._iy) <= 53) || ((_running[i]._y + _running[i]._iy) >= 120))
				_running[i]._iy = -(_running[i]._iy);

			byte frame = 0;
			if (_running[i]._ix < 0)
				frame = _running[i]._frame;
			else
				frame = _running[i]._frame + 7;
			define(_running[i]._x, _running[i]._y, frame, 0, 0, 1, false, true);

			if (_running[i]._frameDelay == 0) {
				_running[i]._frame++;
				if (_running[i]._frame == _running[i]._tooHigh)
					_running[i]._frame = _running[i]._lowest;
				_running[i]._frameDelay = kFrameDelayMax;
				_running[i]._y += _running[i]._iy;
			} else
				_running[i]._frameDelay--;

			if (((_running[i]._x + _running[i]._ix) <= 0) || ((_running[i]._x + _running[i]._ix) >= 620))
				turnAround(i, true);

			_running[i]._x += _running[i]._ix;
		}
	}
}

void ShootEmUp::updateTime() {
	warning("STUB: ShootEmUp::updateTime()");
}

void ShootEmUp::hitPeople() {
	if (_count321 != 0)
		return;

	for (int i = 0; i < 99; i++) {
		if ((_sprites[i]._missile) && (_sprites[i]._x != kFlag)) {
			for (int j = 0; j < 4; j++) {

				bool overlaps = overlap(_sprites[i]._x, _sprites[i]._y, _sprites[i]._x + 7, _sprites[i]._y + 10,
					_running[j]._x, _running[j]._y, _running[j]._x + 17, _running[j]._y + 24);

				if ((_running[j]._x != kFlag) && (overlaps)) {
					_vm->_sound->playNote(7177, 1);
					_sprites[i]._x = kFlag;
					gain(-5);
					define(_running[j]._x + 20, _running[j]._y + 3, 34 + _vm->_rnd->getRandomNumber(5), 1, 3, 9, false, true); // Oof!
					define(_sprites[i]._x, _sprites[i]._y, 83, 1, 0, 17, false, true); // Oops!
				}
			}
		}
	}
}

void ShootEmUp::escapeCheck() {
	if (_count321 != 0)
		return;

	if (_escapeCount > 0) {
		_escapeCount--;
		return;
	}

	// Escape_count = 0; now what ?

	if (_escaping) {
		if (_gotOut) {
			newEscape();
			_escaping = false;
			_vm->_graphics->seuDrawPicture(_escapeStock * 90 + 20, 30, kStocks + 4);
		} else {
			_vm->_graphics->seuDrawPicture(_escapeStock * 90 + 20, 30, kStocks + 5);
			_escapeCount = 20;
			_gotOut = true;
			define(_escapeStock * 90 + 20, 50, 25, 0, 2, 17, false, true); // Escaped!
			gain(-10);
			_hasEscaped[_escapeStock] = true;

			_howManyHaveEscaped++;

			if (_howManyHaveEscaped == 7)
				_time = 0;
		}
	} else {
		_escapeStock = getStockNumber(_vm->_rnd->getRandomNumber(6));
		_escaping = true;
		_gotOut = false;
		_vm->_graphics->seuDrawPicture(_escapeStock * 90 + 20, 30, kStocks + 2); // Smiling!
		_escapeCount = 200;
	}
}

void ShootEmUp::check321() {
	warning("STUB: ShootEmUp::check321()");
}

} // End of namespace Avalanche