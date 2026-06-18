/* ==========================================================================
 *
 * iMWRAP V6 - A modern iMuse implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#ifndef IMUSE_EXPORT_H
#define IMUSE_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(IMUSE_DLL)
#    if defined(IMUSE_BUILD_DLL)
#      define IMUSE_API __declspec(dllexport)
#    else
#      define IMUSE_API __declspec(dllimport)
#    endif
#  else
#    define IMUSE_API
#  endif
#elif defined(IMUSE_DLL)
#  define IMUSE_API __attribute__((visibility("default")))
#else
#  define IMUSE_API
#endif

#endif
