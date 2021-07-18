/*
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include <ardour/session.h>

#include "varispeed_dialog.h"

#include "ardour_ui.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Gtk;

VarispeedDialog::VarispeedDialog ()
	: ArdourDialog (_("Varispeed"))
	, _semitones_adjustment (0.0, -12.0, 12.0, 1.0, 4.0)
	, _cents_adjustment (0.0, -100.0, 100.0, 1.0, 10.0)
	, _semitones_spinner (_semitones_adjustment)
	, _cents_spinner (_cents_adjustment)
{
	set_modal (false);

	Table* t = manage (new Table (3, 2));
	t->set_row_spacings (6);
	t->set_col_spacings (6);

	int    r = 0;
	Label* l = manage (new Label (_("Semitones:"), ALIGN_LEFT, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_semitones_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	l = manage (new Label (_("Cents:"), ALIGN_LEFT, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_cents_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	get_vbox ()->set_spacing (6);
	get_vbox ()->pack_start (*t, false, false);

	_semitones_spinner.set_can_focus (false);
	_cents_spinner.set_can_focus (false);

	_semitones_spinner.signal_changed ().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_speed));
	_cents_spinner.signal_changed ().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_speed));

	show_all_children ();
}

bool
VarispeedDialog::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance ()->main_window ());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

void
VarispeedDialog::reset ()
{
	_semitones_spinner.set_value (0);
	_cents_spinner.set_value (0);
}

void
VarispeedDialog::apply_speed ()
{
	int cents = _semitones_spinner.get_value () * 100 + _cents_spinner.get_value ();

	double speed = pow (2.0, ((double)cents / (double)1200.0));

	if (_session) {
		_session->set_default_play_speed (speed);
	}
}

void
VarispeedDialog::on_hide ()
{
	_session->set_default_play_speed (1.0);
	ArdourDialog::on_hide ();
}
