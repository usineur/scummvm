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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "buried/buried.h"
#include "buried/graphics.h"
#include "buried/video_window.h"

#include "common/system.h"
#include "graphics/surface.h"
#include "video/avi_decoder.h"

namespace Buried {

VideoWindow::VideoWindow(BuriedEngine *vm, Window *parent) : Window(vm, parent), _video(0), _mode(kModeClosed), _lastFrame(0) {
	_vm->addVideo(this);
	_needsPalConversion = false;
	_ownedFrame = 0;
}

VideoWindow::~VideoWindow() {
	closeVideo();
	_vm->removeVideo(this);
}

bool VideoWindow::playVideo() {
	if (!_video)
		return false;

	if (_video->isPlaying())
		return true;

	_video->start();
	_mode = kModePlaying;
	return true;
}

bool VideoWindow::playToFrame(int frame) {
	if (!_video)
		return false;

	_video->setEndFrame(frame);

	if (_video->isPlaying())
		return true;

	_video->start();
	_mode = kModePlaying;
	return true;
}

bool VideoWindow::seekToFrame(int frame) {
	if (!_video)
		return false;

	return _video->seekToFrame(frame);
}

void VideoWindow::stopVideo() {
	if (_video) {
		_video->stop();
		_mode = kModeStopped;
	}
}

int VideoWindow::getCurFrame() {
	if (_video)
		return _video->getCurFrame() + 1;

	return -1;
}

int VideoWindow::getFrameCount() {
	if (_video)
		return _video->getFrameCount();

	return 0;
}

bool VideoWindow::openVideo(const Common::String &fileName) {
	closeVideo();

	_video = new Video::AVIDecoder();

	if (!_video->loadFile(fileName)) {
		closeVideo();
		return false;
	}

	if (!_vm->isTrueColor()) {
		Graphics::PixelFormat videoFormat = _video->getPixelFormat();

		if (videoFormat.bytesPerPixel == 1) {
			_needsPalConversion = true;
		} else {
			_video->setDitheringPalette(_vm->_gfx->getDefaultPalette());
			_needsPalConversion = false;
		}
	}

	_mode = kModeOpen;
	_rect.right = _rect.left + _video->getWidth();
	_rect.bottom = _rect.top + _video->getHeight();
	return true;
}

void VideoWindow::closeVideo() {
	if (_video) {
		delete _video;
		_video = 0;
		_mode = kModeClosed;
		_lastFrame = 0;
		_rect = Common::Rect();

		if (_ownedFrame) {
			_ownedFrame->free();
			delete _ownedFrame;
			_ownedFrame = 0;
		}
	}
}

void VideoWindow::updateVideo() {
	if (_video) {
		if (_video->needsUpdate()) {
			// Store the frame for later
			const Graphics::Surface *frame = _video->decodeNextFrame();
			if (frame) {
				if (_ownedFrame) {
					_ownedFrame->free();
					delete _ownedFrame;
					_ownedFrame = 0;
				}

				if (_vm->isTrueColor()) {
					// Convert to the screen format if necessary
					if (frame->format == g_system->getScreenFormat()) {
						_lastFrame = frame;
					} else {
						_ownedFrame = frame->convertTo(g_system->getScreenFormat(), _video->getPalette());
						_lastFrame = _ownedFrame;
					}
				} else {
					if (_needsPalConversion) {
						// If it's a palette video, ensure it's using the screen palette
						_ownedFrame = _vm->_gfx->remapPalettedFrame(frame, _video->getPalette());
						_lastFrame = _ownedFrame;
					} else {
						// Assume it's in the right format from dithering
						_lastFrame = frame;
					}
				}
			}

			// Invalidate the window so it gets updated
			invalidateWindow(false);
		}

		if (_video->isPlaying() && _video->endOfVideo()) {
			_video->stop();
			_mode = kModeStopped;
		}
	}
}

void VideoWindow::onPaint() {
	if (_lastFrame) {
		Common::Rect absoluteRect = getAbsoluteRect();
		_vm->_gfx->blit(_lastFrame, absoluteRect.left, absoluteRect.top);
	}
}

} // End of namespace Buried