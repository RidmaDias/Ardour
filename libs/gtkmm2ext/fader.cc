/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: fastmeter.h 570 2006-06-07 21:21:21Z sampo $
*/


#include <iostream>

#include "pbd/stacktrace.h"

#include "gtkmm2ext/fader.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/utils.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

#define CORNER_RADIUS 4
#define CORNER_SIZE   2
#define CORNER_OFFSET 1
#define FADER_RESERVE 5


static void get_closest_point_on_line(double xa, double ya, double xb, double yb, double xp, double yp, double& xl, double& yl )
{
	// Storing vector A->B
    double a_to_b_x = xb - xa;
	double a_to_b_y = yb - ya;
    
	// Storing vector A->P
    double a_to_p_x = xp - xa;
	double a_to_p_y = yp - ya;
    

    // Basically finding the squared magnitude
    // of a_to_b
    double atb2 = a_to_b_x * a_to_b_x + a_to_b_y * a_to_b_y;
 
    // The dot product of a_to_p and a_to_b
    double atp_dot_atb = a_to_p_x * a_to_b_x + a_to_p_y * a_to_b_y;
    
    // The normalized "distance" from a to
    // your closest point
    double t = atp_dot_atb / atb2;
    
    // The vector perpendicular to a_to_b;
    // This step can also be combined with the next
	double perpendicular_x = -a_to_b_y;
	double perpendicular_y = a_to_b_x;

    // Finding Q, the point "in the right direction"
    // If you want a mess, you can also combine this
    // with the next step.
	double xq = xp + perpendicular_x;
	double yq = yp + perpendicular_y;

    // Add the distance to A, moving
    // towards B
    double x = xa + a_to_b_x * t;
    double y = ya + a_to_b_y * t;

	if ((xa != xb)) {
		if ((x < xa) && (x < xb)) {
			if (xa < xb) {
				x = xa;
				y = ya;
			} else {
				x = xb;
				y = yb;
			}
		} else if ((x > xa) && (x > xb)) {
			if (xb > xa) {
				x = xb;
				y = yb;
			} else {
				x = xa;
				y = ya;
			}
		}
	} else {
		if ((y < ya) && (y < yb)) {
			if (ya < yb) {
				x = xa;
				y = ya;
			} else {
				x = xb;
				y = yb;
			}
		} else if ((y > ya) && (y > yb)) {
			if (yb > ya) {
				x = xb;
				y = yb;
			} else {
				x = xa;
				y = ya;
			}
		}
	}

	xl = x;
	yl = y;
}

Fader::Fader (Gtk::Adjustment& adj,
			  const std::string& face_image_file, 
			  const std::string& handle_image_file,
			  const std::string& active_handle_image_file,
			  int min_pos_x, 
			  int min_pos_y,
			  int max_pos_x,
			  int max_pos_y)
	: adjustment (adj)
	, _min_pos_x (min_pos_x)
	, _min_pos_y (min_pos_y)
	, _max_pos_x (max_pos_x)
	, _max_pos_y (max_pos_y)
	, _default_value (adjustment.get_value())
	, _dragging (false)
{
	PBD::Searchpath spath(ARDOUR::ardour_data_search_path());

	spath.add_subdirectory_to_paths ("icons");

	std::string data_file_path;

	if (PBD::find_file_in_search_path (spath, face_image_file, data_file_path)) {
		_face_pixbuf  = Gdk::Pixbuf::create_from_file (data_file_path);
	} else {
		throw failed_constructor(); 
	}

	if (PBD::find_file_in_search_path (spath, handle_image_file, data_file_path)) {
		_handle_pixbuf  = Gdk::Pixbuf::create_from_file (data_file_path);
	} else {
		throw failed_constructor(); 
	}
	if (PBD::find_file_in_search_path (spath, active_handle_image_file, data_file_path)) {
		_active_handle_pixbuf  = Gdk::Pixbuf::create_from_file (data_file_path);
	} else {
		throw failed_constructor(); 
	}

	update_unity_position ();

	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK|Gdk::SCROLL_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	adjustment.signal_value_changed().connect (mem_fun (*this, &Fader::adjustment_changed));
	adjustment.signal_changed().connect (mem_fun (*this, &Fader::adjustment_changed));
    DrawingArea::set_size_request(_face_pixbuf->get_width(), _face_pixbuf->get_height());
}

Fader::~Fader ()
{
}

bool
Fader::on_expose_event (GdkEventExpose* ev)
{
	Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();
	cairo_t* cr = context->cobj();
    
    cairo_rectangle (cr, 0, 0, get_width(), get_height());
    gdk_cairo_set_source_pixbuf (cr, _face_pixbuf->gobj(), 0, 0);
    cairo_fill (cr);

	get_handle_position (_last_drawn_x, _last_drawn_y);

    cairo_rectangle (cr, 0, 0, get_width(), get_height());
	if (_dragging) {
		gdk_cairo_set_source_pixbuf (cr,
									 _active_handle_pixbuf->gobj(),
									 _last_drawn_x - _active_handle_pixbuf->get_width()/2.0,
									 _last_drawn_y - _active_handle_pixbuf->get_height()/2.0);
	} else {
		gdk_cairo_set_source_pixbuf (cr,
									 _handle_pixbuf->gobj(),
									 _last_drawn_x - _handle_pixbuf->get_width()/2.0,
									 _last_drawn_y - _handle_pixbuf->get_height()/2.0);
	}
    cairo_fill (cr);

	return true;
}

void
Fader::on_size_request (GtkRequisition* req)
{
	req->width = _face_pixbuf->get_width();
	req->height = _face_pixbuf->get_height();
}

void
Fader::on_size_allocate (Gtk::Allocation& alloc)
{
    DrawingArea::on_size_allocate(alloc);
	update_unity_position ();
}

bool
Fader::on_button_press_event (GdkEventButton* ev)
{
	if (ev->type != GDK_BUTTON_PRESS) {
		return true;
	}

	if (ev->button != 1 && ev->button != 2) {
		return false;
	}

	double hx;
	double hy;
	get_handle_position (hx, hy);

	double hw = _handle_pixbuf->get_width();
	double hh = _handle_pixbuf->get_height();

	if ((ev->x < (hx - hw/2)) || (ev->x > (hx + hw/2)) || (ev->y < (hy - hh/2)) || (ev->y > (hy + hh/2))) {
		return false;
	}

	double ev_pos_x;
	double ev_pos_y;
		
	get_closest_point_on_line(_min_pos_x, _min_pos_y,
								_max_pos_x, _max_pos_y, 
								ev->x, ev->y,
								ev_pos_x, ev_pos_y );
	_grab_loc_x = ev_pos_x;
	_grab_loc_y = ev_pos_y;

	add_modal_grab ();
	
	_grab_start_x = _grab_loc_x = ev_pos_x;
	_grab_start_y = _grab_loc_y = ev_pos_y;

	_grab_window = ev->window;
	_dragging = true;
	
	gdk_pointer_grab(ev->window,false,
					 GdkEventMask (Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK),
					 NULL,
					 NULL,
					 ev->time);

	if (ev->button == 2) {
		set_adjustment_from_event (ev);
	}

	queue_draw();
	
	return true;
}

bool
Fader::on_button_release_event (GdkEventButton* ev)
{
	if (_dragging) { //temp
		remove_modal_grab();
		_dragging = false;
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		queue_draw ();
	}
	/*
	double const ev_pos_x = ev->x;
	double const ev_posyx = ev->y;

	switch (ev->button) {
	case 1:
		if (dragging) {
			remove_modal_grab();
			dragging = false;
			gdk_pointer_ungrab (GDK_CURRENT_TIME);

			if (!_hovering) {
				Keyboard::magic_widget_drop_focus();
				queue_draw ();
			}

			if ((ev_pos_x == grab_start_x) && (ev_pos_y == grab_start_y)) {

				// no motion - just a click
				if (ev->state & Keyboard::TertiaryModifier) {
					adjustment.set_value (_default_value);
				} else if (ev->state & Keyboard::GainFineScaleModifier) {
					adjustment.set_value (adjustment.get_lower());
				} else if ((_orien == VERT && ev_pos < display_span()) || (_orien == HORIZ && ev_pos > display_span())) {
					// above the current display height, remember X Window coords
					adjustment.set_value (adjustment.get_value() + adjustment.get_step_increment());
				} else {
					adjustment.set_value (adjustment.get_value() - adjustment.get_step_increment());
				}
			}
			return true;
		} 
		break;
		
	case 2:
		if (dragging) {
			remove_modal_grab();
			dragging = false;
			set_adjustment_from_event (ev);
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			return true;
		}
		break;

	default:
		break;
	}
*/
	return false;

/*
	double const ev_pos = (_orien == VERT) ? ev->y : ev->x;
	
	switch (ev->button) {
	case 1:
		if (dragging) {
			remove_modal_grab();
			dragging = false;
			gdk_pointer_ungrab (GDK_CURRENT_TIME);

			if (!_hovering) {
				Keyboard::magic_widget_drop_focus();
				queue_draw ();
			}

			if (ev_pos == grab_start) {

				// no motion - just a click

				if (ev->state & Keyboard::TertiaryModifier) {
					adjustment.set_value (_default_value);
				} else if (ev->state & Keyboard::GainFineScaleModifier) {
					adjustment.set_value (adjustment.get_lower());
				} else if ((_orien == VERT && ev_pos < display_span()) || (_orien == HORIZ && ev_pos > display_span())) {
					// above the current display height, remember X Window coords
					adjustment.set_value (adjustment.get_value() + adjustment.get_step_increment());
				} else {
					adjustment.set_value (adjustment.get_value() - adjustment.get_step_increment());
				}
			}
			return true;
		} 
		break;
		
	case 2:
		if (dragging) {
			remove_modal_grab();
			dragging = false;
			set_adjustment_from_event (ev);
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			return true;
		}
		break;

	default:
		break;
	}

	return false;
	*/
}

bool
Fader::on_scroll_event (GdkEventScroll* ev)
{
	double scale;
	bool ret = false;

	if (ev->state & Keyboard::GainFineScaleModifier) {
		if (ev->state & Keyboard::GainExtraFineScaleModifier) {
			scale = 0.01;
		} else {
			scale = 0.05;
		}
	} else {
		scale = 1;
	}

	switch (ev->direction) {
	case GDK_SCROLL_RIGHT:
	case GDK_SCROLL_UP:
		adjustment.set_value (adjustment.get_value() + (adjustment.get_page_increment() * scale));
		ret = true;
		break;
	case GDK_SCROLL_LEFT:
	case GDK_SCROLL_DOWN:
		adjustment.set_value (adjustment.get_value() - (adjustment.get_page_increment() * scale));
		ret = true;
		break;
	default:
		break;
	}
	return ret;
}

bool
Fader::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_dragging) {
		double ev_pos_x;
		double ev_pos_y;
		
		get_closest_point_on_line(_min_pos_x, _min_pos_y,
								  _max_pos_x, _max_pos_y, 
								  ev->x, ev->y,
								  ev_pos_x, ev_pos_y );

		_grab_loc_x = ev_pos_x;
		_grab_loc_y = ev_pos_y;

		if (ev->window != _grab_window) {
			_grab_window = ev->window;
			return true;
		}
		
		double const fract = sqrt((ev_pos_x - _min_pos_x) * (ev_pos_x - _min_pos_x) +
								  (ev_pos_y - _min_pos_y) * (ev_pos_y - _min_pos_y)) /
							 sqrt((_max_pos_x - _min_pos_x) * (_max_pos_x - _min_pos_x) +
								  (_max_pos_y - _min_pos_y) * (_max_pos_y - _min_pos_y));
		
		adjustment.set_value (adjustment.get_lower() + (adjustment.get_upper() - adjustment.get_lower()) * fract);
	}
	return true;
}

void
Fader::adjustment_changed ()
{
    double handle_x;
	double handle_y;
	get_handle_position (handle_x, handle_y);

	if ((handle_x != _last_drawn_x) || (handle_y != _last_drawn_y)) {
		queue_draw ();
	}
}

/** @return pixel offset of the current value from the right or bottom of the fader */
void
Fader::get_handle_position (double& x, double& y)
{
	double fract = (adjustment.get_value () - adjustment.get_lower()) / ((adjustment.get_upper() - adjustment.get_lower()));

	x = _min_pos_x + (_max_pos_x - _min_pos_x) * fract;
	y = _min_pos_y + (_max_pos_y - _min_pos_y) * fract;
}

bool
Fader::on_enter_notify_event (GdkEventCrossing*)
{
	_hovering = true;
	Keyboard::magic_widget_grab_focus ();
	queue_draw ();
	return false;
}

bool
Fader::on_leave_notify_event (GdkEventCrossing*)
{
	if (!_dragging) {
		_hovering = false;
		Keyboard::magic_widget_drop_focus();
		queue_draw ();
	}
	return false;
}

void
Fader::set_adjustment_from_event (GdkEventButton* ev)
{
/*	double span = 
	double fract = (_orien == VERT) ? (1.0 - (ev->y / span)) : (ev->x / span);

	fract = min (1.0, fract);
	fract = max (0.0, fract);

	adjustment.set_value (fract * (adjustment.get_upper () - adjustment.get_lower ()));
	*/
}

void
Fader::set_default_value (float d)
{
	_default_value = d;
	update_unity_position ();
}

void
Fader::update_unity_position ()
{
}
