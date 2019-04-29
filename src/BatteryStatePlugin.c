/*
 * A battery monitor plugin for LXPanel, which shows a battery icon and the
 * state of charge in percent
 *
 * Copyright (C) 2018 by Johannes Roith <johannesr8484@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * The model for this plugin are the temperature and battery monitor applet
 * from the lxpanel project. To use the battery icon, install the awesome-font
 *
 * This plugin monitors battery usage on ACPI-enabled systems by reading the
 * battery information found in /sys/class/power_supply. The update interval is
 * user-configurable and defaults to 3 second.
 *
 * The battery's remaining life is estimated from its current charge and current
 * rate of discharge. The user may configure an alarm command to be run when
 * their estimated remaining battery life reaches a certain level.
 */

/* FIXME:
 *  Here are somethings need to be improved:
 *  1. Add some space between the icon and the percentage text
 *  2. Replace the checkboxes for selection if text and/or icon should be displayed
 *     with a drop-down list, which only allows three states: 
 *      -show icon only
 *      -show percentage text only
 *      -show icon and text
*/

#include <lxpanel/plugin.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>
#include "batt_sys.h"

#define ICON_FULL			0xf240
#define ICON_THREE_QUARTERS	0xf241
#define ICON_HALF			0xf242
#define ICON_QUARTER		0xf243
#define ICON_EMPTY			0xf244

const int batteryIcons[]={
	ICON_EMPTY,			// 0-9%
	ICON_QUARTER,		// 10 
	ICON_QUARTER,		// -
	ICON_QUARTER,		// 39%
	ICON_HALF,			// 40 -
	ICON_HALF,			// 59%
	ICON_THREE_QUARTERS,// 60
	ICON_THREE_QUARTERS,// -
	ICON_THREE_QUARTERS,// 89%
	ICON_FULL,			// 90 - 100%
	ICON_FULL
};
	
	

//Struct to describe the BatteryStatePlugin 
typedef struct
{
	LXPanel * panel;		/* Back pointer to Panel */
	GtkWidget * label;		/* The label */
	GString *tip;			/* string for tooltip */
	unsigned int timer;		/* reference to the running timer */
	battery *b;				/* The battery to be viewed */
	gboolean bShowedWarning;/* did i already have shown the warning */
	
	//configs
	int iBatteryNumber;				/* number of the battery*/
	gboolean bShowIcon;				/* show battery icon */
	gboolean bShowText;				/* show battery state in percentage */
	gboolean bColoredText;			/* should the text be colored like the battery icon */
	GdkColor ColorCharging;			/* color of the icon, if battery is charging */
	GdkColor ColorDischarging;		/* color of the icon, if battery is discharging */
	char * chargingColor;
	char * dischargingColor;
	char * warningText;
	unsigned int warningPercent;
	config_setting_t *settings;

}BatteryStatePlugin;

static void batstat_destructor(gpointer user_data);

/* Make a tooltip string, and display remaining charge time if the battery
   is charging or remaining life if it's discharging */
static void update_tooltip(BatteryStatePlugin* BatStat, gboolean isCharging)
{
    battery *b = BatStat->b;
	g_string_truncate(BatStat->tip, 0);
	
    if (b == NULL)
        return;

    if (isCharging) {
        if (BatStat->b->seconds > 0) {
            int hours = BatStat->b->seconds / 3600;
            int left_seconds = BatStat->b->seconds - 3600 * hours;
            int minutes = left_seconds / 60;
            g_string_printf(BatStat->tip, "Battery %d: %d%% charged, %d:%02d until full", BatStat->iBatteryNumber, BatStat->b->percentage, hours, minutes );
        }
        else
            goto _charged;
    } else {
        /* if we have enough rate information for battery */
        if (BatStat->b->percentage != 100) {
            int hours = BatStat->b->seconds / 3600;
            int left_seconds = BatStat->b->seconds - 3600 * hours;
            int minutes = left_seconds / 60;
            g_string_printf(BatStat->tip, "Battery %d: %d%% charged, %d:%02d left", BatStat->iBatteryNumber, BatStat->b->percentage, hours, minutes );
        } else {
_charged:
			g_string_printf(BatStat->tip, "Battery %d: %d%% charged", BatStat->iBatteryNumber, BatStat->b->percentage);
        }
    }
    gtk_widget_set_tooltip_text(BatStat->label, BatStat->tip->str);

}

/*
 * reads the SoC and updates the gtk label
 */
static void update_display(BatteryStatePlugin * plugin)
{
	char buffer[60]="\0";
	char strColorStart[30];
	char strColorEnd[]="</span>";
	char strIcon[10];
	char strPercentage[10];
	GdkColor Color;
	
	//Update Battery State
	if(battery_update(plugin->b) == NULL)
	{	//Error
		snprintf(buffer, sizeof(buffer), "n.a.");
		gtk_label_set_markup(GTK_LABEL(plugin->label), buffer);
		gtk_widget_set_tooltip_text(plugin->label, "No battery found");
		return;
		
	}
	if(!(plugin->bShowIcon || plugin->bShowText))
	{	//there is nothing to show -> show n.s.
		snprintf(buffer, sizeof(buffer), "n.s.");
		gtk_label_set_markup(GTK_LABEL(plugin->label), buffer);
		gtk_widget_set_tooltip_text(plugin->label, "Nothing selected to be displayed");
		return;
	}
	
	//set battery icon color
	if(battery_is_charging(plugin->b)==FALSE)
		Color=plugin->ColorDischarging;
	else
		Color=plugin->ColorCharging;		/* maybe helps style=\"padding-right:3px\" */
	snprintf(strColorStart, sizeof(strColorStart), "<span color=\"#%06x\">", gcolor2rgb24(&Color));		//set color
	snprintf(strIcon, sizeof(strIcon), "&#%d;", batteryIcons[plugin->b->percentage/10]);				//set Icon
	snprintf(strPercentage, sizeof(strPercentage), "%d%c", plugin->b->percentage, 37);					//set SoC
	/* FIXME
	 * Add some space between the icon and the percentage text
	 */
	//Update the display
	if(!(plugin->bShowIcon) && plugin->bShowText && !(plugin->bColoredText))		//only white text
		snprintf(buffer, sizeof(buffer), "%s", strPercentage);
	else
	{
		strcpy(buffer, strColorStart);
		if(!(plugin->bShowIcon))
		{//only colored text
			strcat(buffer, strPercentage);
			strcat(buffer, strColorEnd);
		}
		else
		{	
			if(!(plugin->bShowText))
			{	//only Icon
				strcat(buffer, strIcon);
				strcat(buffer, strColorEnd);
			}
			else
			{
				if(plugin->bColoredText)
				{	//Text and Icon Colored
					strcat(buffer, strIcon);
					strcat(buffer, strPercentage);
					strcat(buffer, strColorEnd);
				}
				else
				{	//Only colored Icon
					strcat(buffer, strIcon);
					strcat(buffer, strColorEnd);
					strcat(buffer, strPercentage);
				}
			}
		}
	}
	gtk_label_set_markup(GTK_LABEL(plugin->label), buffer);
	
	//Show Battery Warning if Battery is almost empty
	if((plugin->b->percentage <= plugin->warningPercent) && (plugin->bShowedWarning == FALSE)){
		snprintf(buffer, sizeof(buffer), "notify-send \"%s\" --icon=battery-caution", plugin->warningText);
		system(buffer);
		plugin->bShowedWarning = TRUE;
	}
	if(plugin->b->percentage > plugin->warningPercent)
		plugin->bShowedWarning = FALSE;
	
	update_tooltip(plugin, battery_is_charging(plugin->b));
}


/*
 * calls update function to get SoC
 * returns TRUE while g_source isn't destroyed
 */
static gboolean update_display_timeout(gpointer user_data)
{
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    update_display(user_data);
    return TRUE; /* repeat later */
}


/*
 * Constructor of BatteryStatePlugin
 */
GtkWidget *batstat_constructor(LXPanel *panel, config_setting_t *settings)
{
	int tmp_int;
	const char *str;
	
	BatteryStatePlugin *BatStat;
	
	//Allocate new Instance of BatteryStatePlugin
	BatStat=g_new0(BatteryStatePlugin, 1);
	BatStat->panel = panel;
	
	BatStat->tip = g_string_new(NULL);
	
	/* get requested battery */
    if (config_setting_lookup_int(settings, "BatteryNumber", &tmp_int))
    	BatStat->iBatteryNumber = MAX(0, tmp_int);
    	
    BatStat->b = battery_get(1);
    BatStat->iBatteryNumber=BatStat->b->battery_num;
    	
	GtkWidget *p;
	
	//Allocate new Instance of gtk event box
	p = gtk_event_box_new();
	lxpanel_plugin_set_data(p, BatStat, batstat_destructor);
	gtk_widget_set_has_window(p, FALSE);
	gtk_container_set_border_width( GTK_CONTAINER(p), 2 );
	
	BatStat->label = gtk_label_new("ww");
	gtk_container_add(GTK_CONTAINER(p), BatStat->label);
	
	BatStat->chargingColor = BatStat->dischargingColor = BatStat->warningText = NULL;
	
	//Configuration of GTK_Label
	if (config_setting_lookup_int(settings, "ShowIcon", &tmp_int))
        BatStat->bShowIcon = (tmp_int != 0);
    if (config_setting_lookup_int(settings, "ShowText", &tmp_int))
        BatStat->bShowText = (tmp_int != 0);
    if (config_setting_lookup_int(settings, "ColoredText", &tmp_int))
        BatStat->bColoredText = (tmp_int != 0);
    if (config_setting_lookup_string(settings, "ChargingColor", &str))
        BatStat->chargingColor = g_strdup(str);
    if (config_setting_lookup_string(settings, "DischargingColor", &str))
        BatStat->dischargingColor = g_strdup(str);
    if (config_setting_lookup_string(settings, "WarningText", &str))
        BatStat->warningText = g_strdup(str);
    if (config_setting_lookup_int(settings, "WarningPercent", &tmp_int))
        BatStat->warningPercent = MAX(0, tmp_int);
    BatStat->bShowedWarning=FALSE;
    
    /*Default values*/
    if(! BatStat->chargingColor)
        BatStat->chargingColor = g_strdup("#28f200");
	if(! BatStat->dischargingColor)
        BatStat->dischargingColor = g_strdup("#ffffff");
    if(! BatStat->warningText)
    	BatStat->warningText = g_strdup("Battery almost empty");
        
    gdk_color_parse(BatStat->chargingColor, &BatStat->ColorCharging);
    gdk_color_parse(BatStat->dischargingColor, &BatStat->ColorDischarging);
	BatStat->settings = settings;
	//Show label
	gtk_widget_show(BatStat->label);
	
	//timer
	update_display(BatStat);
	BatStat->timer = g_timeout_add_seconds(3, (GSourceFunc) update_display_timeout,  (gpointer)BatStat);
	
	//Return Plugin
	return p;
}

/*
 * Destructor of BatteryStatePlugin
 */
static void batstat_destructor(gpointer user_data)
{
	BatteryStatePlugin *BatStat = (BatteryStatePlugin *) user_data;	
	battery_free(BatStat->b);
	g_source_remove(BatStat->timer);
	g_string_free(BatStat->tip, TRUE);
	g_free(BatStat->warningText);
	g_free(BatStat->chargingColor);
	g_free(BatStat->dischargingColor);
	g_free(BatStat);
	return;
}


static gboolean applyConfig(gpointer user_data)
{
	BatteryStatePlugin *batstat = lxpanel_plugin_get_data(user_data);

    // Update the battery we monitor
    battery_free(batstat->b);
    batstat->b = battery_get(batstat->iBatteryNumber);
    
    if (batstat->chargingColor &&
            gdk_color_parse(batstat->chargingColor, &batstat->ColorCharging))
        config_group_set_string(batstat->settings, "ChargingColor", batstat->chargingColor);
    if (batstat->dischargingColor &&
            gdk_color_parse(batstat->dischargingColor, &batstat->ColorDischarging))
        config_group_set_string(batstat->settings, "DischargingColor", batstat->dischargingColor);
    
    if (batstat->warningText == NULL)
    	batstat->warningText= g_strconcat("Battery low", NULL);
    
    gdk_color_parse(batstat->chargingColor, &batstat->ColorCharging);
    gdk_color_parse(batstat->dischargingColor, &batstat->ColorDischarging);
    config_group_set_string(batstat->settings, "WarningText", batstat->warningText);
    config_group_set_int(batstat->settings, "WarningPercent", batstat->warningPercent);
    config_group_set_int(batstat->settings, "ShowIcon", batstat->bShowIcon);
    config_group_set_int(batstat->settings, "ShowText", batstat->bShowText);
    config_group_set_int(batstat->settings, "ColoredText", batstat->bColoredText);
    
    update_display(batstat);
	return TRUE;
}

/* FIXME
 *  Replace the checkboxes for selection if text and/or icon should be displayed
 *  with a drop-down list, which only allows three states: 
 *    -show icon only
 *    -show percentage text only
 *    -show icon and text
 */
static GtkWidget *config(LXPanel *panel, GtkWidget *p) {
    BatteryStatePlugin *b = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Battery State Plugin"),
            panel, applyConfig, p,
            _("Show Battery Icon"), &b->bShowIcon, CONF_TYPE_BOOL,
            _("Show battery state in percentage"), &b->bShowText, CONF_TYPE_BOOL,
            _("Color text like battery icon"), &b->bColoredText, CONF_TYPE_BOOL,
            _("Charging color"), &b->chargingColor, CONF_TYPE_STR,
            _("Discharging color"), &b->dischargingColor, CONF_TYPE_STR,
            _("Battery Warning Text"), &b->warningText, CONF_TYPE_STR,
            _("State of Charge in percent to show warning"), &b->warningPercent, CONF_TYPE_INT,
            _("Number of battery to monitor"), &b->iBatteryNumber, CONF_TYPE_INT,
            NULL);
}


FM_DEFINE_MODULE(lxpanel_gtk, BatteryStatePlugin)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
   .name = "BatteryStatePlugin",
   .description = "Shows the State of charge of the battery",

   // assigning our functions to provided pointers.
   .new_instance = batstat_constructor,
   .config=config
};
