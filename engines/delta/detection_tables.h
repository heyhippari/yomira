/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
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
 */

namespace Delta {

const PlainGameDescriptor deltaGames[] = {
	{ "onelover", "One\u2606Lover!! ~Miku-chan no Koibito~" },
	{ "onegaitasukete", "Onegai Tasukete! ~Soto ni Detara Yararechau~" },
	{ 0, 0 }
};

const ADGameDescription gameDescriptions[] = {
	{
		"onelover",
		"Download Edition",
		AD_ENTRY1s("OLV_DLN.SD", "24e966a2b11fcfdff8b4de8a62797bd4", 421941),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_UNSTABLE,
		GUIO1(GUIO_NONE)
	},

	{
		"onegaitasukete",
		"Package Edition",
		AD_ENTRY1s("PHMN.SD", "7b8ec8c790dd6083470cb99b08500852", 645551),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_UNSTABLE,
		GUIO1(GUIO_NONE)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace Delta
