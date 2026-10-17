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

namespace Illusion {

const PlainGameDescriptor illusionGames[] = {
	{ "sexybeach2", "Sexy Beach 2" },
	{ 0, 0 }
};

const ADGameDescription gameDescriptions[] = {
	{
		"sexybeach2",
		nullptr,
		AD_ENTRY3s("Data/sb2_000.tbl", "eab8bf3e7a2a4927f3835e324f287eb0", 803,
		           "Data/sb2_000.pp", "8d138f07499ea13eaf377d0e81b8d327", 24515660,
		           "Data/sune.rrr", "ca1ba7ddcf82e5d79343050e46858200", 12),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_UNSTABLE,
		GUIO1(GUIO_NONE)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace Illusion
