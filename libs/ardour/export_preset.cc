/*
 * Copyright (C) 2008-2009 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/export_preset.h"

#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;

ExportPreset::ExportPreset (Session& s, string const& filename)
	: session (s)
	, local (0)
{
	if (filename.empty ()) {
		return;
	}

	global.read (filename);

	XMLNode* root;
	if ((root = global.root())) {
		std::string str;
		if (root->get_property ("id", str)) {
			set_id (str);
		}
		if (root->get_property ("name", str)) {
			set_name (str);
		}

		XMLNode * instant_xml = get_instant_xml ();
		if (instant_xml) {
			XMLNode * instant_copy = new XMLNode (*instant_xml);
			set_local_state (*instant_copy);
		}
	}
}

ExportPreset::~ExportPreset ()
{
	delete local;
}

void
ExportPreset::set_name (string const & name)
{
	_name = name;

	XMLNode * node;
	if ((node = global.root())) {
		node->set_property ("name", name);
	}
	if (local) {
		local->set_property ("name", name);
	}
}

void
ExportPreset::set_id (string const & id)
{
	_id = id;

	XMLNode * node;
	if ((node = global.root())) {
		node->set_property ("id", id);
	}
	if (local) {
		local->set_property ("id", id);
	}
}

void
ExportPreset::set_global_state (XMLNode & state)
{
	delete global.root ();
	global.set_root (&state);

	set_id (_id.to_s());
	set_name (_name);
}

void
ExportPreset::set_local_state (XMLNode & state)
{
	delete local;
	local = &state;

	set_id (_id.to_s());
	set_name (_name);
}

void
ExportPreset::save (std::string const & filename)
{
	save_instant_xml ();

	if (global.root()) {
		global.set_filename (filename);
		global.write ();
	}
}

void
ExportPreset::remove_local () const
{
	remove_instant_xml ();
}

XMLNode *
ExportPreset::get_instant_xml () const
{
	XMLNode * instant_xml;

	if ((instant_xml = session.instant_xml ("ExportPresets"))) {
		XMLNodeList children = instant_xml->children ("ExportPreset");
		for (XMLNodeList::iterator it = children.begin(); it != children.end(); ++it) {
			XMLProperty const * prop;
			if ((prop = (*it)->property ("id")) && _id == PBD::UUID(prop->value())) {
				return *it;
			}
		}
	}

	return 0;
}

void
ExportPreset::save_instant_xml () const
{
	if (!local) { return; }

	/* First remove old, then add new */

	remove_instant_xml ();

	XMLNode * instant_xml;
	if ((instant_xml = session.instant_xml ("ExportPresets"))) {
		instant_xml->add_child_copy (*local);
	} else {
		instant_xml = new XMLNode ("ExportPresets");
		instant_xml->add_child_copy (*local);
		session.add_instant_xml (*instant_xml, false);
	}
}

void
ExportPreset::remove_instant_xml () const
{
	XMLNode * instant_xml;
	if ((instant_xml = session.instant_xml ("ExportPresets"))) {
		instant_xml->remove_nodes_and_delete ("id", _id.to_s());
	}
}
