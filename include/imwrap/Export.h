/* ==========================================================================
 *
 * iMWrap v6 - A modern iMWrap implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#ifndef IMWRAP_EXPORT_H
#define IMWRAP_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(IMWRAP_DLL)
#    if defined(IMWRAP_BUILD_DLL)
#      define IMWRAP_API __declspec(dllexport)
#    else
#      define IMWRAP_API __declspec(dllimport)
#    endif
#  else
#    define IMWRAP_API
#  endif
#elif defined(IMWRAP_DLL)
#  define IMWRAP_API __attribute__((visibility("default")))
#else
#  define IMWRAP_API
#endif

#endif
