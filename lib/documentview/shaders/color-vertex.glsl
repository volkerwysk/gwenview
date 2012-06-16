/*
Gwenview: an image viewer
Copyright 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
#ifdef GL_ES
precision highp float;
#endif
uniform mat4 modelviewProjection;
attribute vec4 vertex;

void main() {
    gl_Position = modelviewProjection*vec4(vertex.xy, 0.0, 1.0);
}
