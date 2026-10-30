/** \file
 * Soup Engine as a Magic Lantern module (EOS 7D, ML lua_fix).
 *
 * Film-in-ML bodies rebuild Magic Lantern's autoexec with the Film page
 * built in. The 7D keeps the stock lua_fix autoexec instead: everything
 * Soup needs from Magic Lantern is exported to modules, and menu_add()
 * creates the Film tab on its own (a tab without an icon is drawn with
 * the first letter of its name, so it still shows up as "F").
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */
#include <module.h>
#include <dryos.h>

/* film_menu.c: adds the Film tab and starts the develop task. */
extern void film_menu_init(void);

static unsigned int soup_init()
{
    film_menu_init();
    return 0;
}

static unsigned int soup_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(soup_init)
    MODULE_DEINIT(soup_deinit)
MODULE_INFO_END()
