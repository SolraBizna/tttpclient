# Copyright (C) 2015 Solra Bizna
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

# TODO: parametrize
bin/tttpclient-release$(EXE): obj/tttpclient.o obj/lsx_bzero.o obj/lsx_random.o obj/lsx_twofish.o obj/lsx_sha256.o obj/tttp_common.o obj/tttp_client.o obj/display.o obj/sdlsoft_display.o obj/font.o obj/blend_table.o obj/charconv.o obj/modal_error.o obj/mac16.o obj/break_lines.o obj/widget.o obj/container.o obj/loose_text.o obj/labeled_field.o obj/secure_labeled_field.o obj/button.o obj/modal_confirm.o obj/modal_info.o obj/connection_dialog.o obj/connection.o obj/pkdb.o obj/key_manage_dialog.o
bin/tttpclient-debug$(EXE): $(patsubst %.o,%.debug.o,obj/tttpclient.o obj/lsx_bzero.o obj/lsx_random.o obj/lsx_twofish.o obj/lsx_sha256.o obj/tttp_common.o obj/tttp_client.o obj/display.o obj/sdlsoft_display.o obj/font.o obj/blend_table.o obj/charconv.o obj/modal_error.o obj/mac16.o obj/break_lines.o obj/widget.o obj/container.o obj/loose_text.o obj/labeled_field.o obj/secure_labeled_field.o obj/button.o obj/modal_confirm.o obj/modal_info.o obj/connection_dialog.o obj/connection.o obj/pkdb.o obj/key_manage_dialog.o)
