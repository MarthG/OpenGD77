/*
 * Copyright (C)2019 Roger Clark. VK3KYY / G4KYF
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <fw_codeplug.h>
#include <user_interface/menuSystem.h>
#include <user_interface/menuUtilityQSOData.h>
#include "fw_main.h"
#include "fw_settings.h"

static void updateScreen();
static void handleEvent(int buttons, int keys, int events);
static struct_codeplugContact_t contact;
static int contactCallType;
static int menuContactListDisplayState;
static int menuContactListTimeout;
static int menuContactListOverrideState = 0;

enum MENU_CONTACT_LIST_STATE
{
	MENU_CONTACT_LIST_DISPLAY = 0,
	MENU_CONTACT_LIST_CONFIRM,
	MENU_CONTACT_LIST_DELETED,
	MENU_CONTACT_LIST_TG_IN_RXGROUP
};

static const char *calltypeName[] = { "Group Call", "Private Call", "All Call" };

void reloadContactList(void)
{
	gMenusEndIndex = codeplugContactsGetCount(contactCallType);
	gMenusCurrentItemIndex = 0;
	if (gMenusEndIndex > 0) {
		contactListContactIndex = codeplugContactGetDataForNumber(gMenusCurrentItemIndex+1, contactCallType, &contactListContactData);
	}
}

int menuContactList(int buttons, int keys, int events, bool isFirstRun)
{
	if (isFirstRun)
	{
		if (menuContactListOverrideState == 0) {
			contactCallType = CONTACT_CALLTYPE_TG;
			fw_reset_keyboard();
			reloadContactList();
			menuContactListDisplayState = MENU_CONTACT_LIST_DISPLAY;
		}
		else
		{
			codeplugContactGetDataForIndex(contactListContactIndex, &contactListContactData);
			menuContactListDisplayState = menuContactListOverrideState;
			menuContactListOverrideState = 0;
		}
		updateScreen();
	}
	else
	{
		if ((events!=0 && keys!=0) || menuContactListTimeout > 0)
		{
			handleEvent(buttons, keys, events);
		}
	}
	return 0;
}

static void updateScreen(void)
{
	char nameBuf[17];
	int mNum;
	int idx;

	UC1701_clearBuf();

	switch (menuContactListDisplayState)
	{
	case MENU_CONTACT_LIST_DISPLAY:
		menuDisplayTitle((char *) calltypeName[contactCallType]);

		if (gMenusEndIndex == 0)
		{
			UC1701_printCentered(32, "Empty List", UC1701_FONT_8x16);
		}
		else
		{
			for (int i = -1; i <= 1; i++)
			{
				mNum = menuGetMenuOffset(gMenusEndIndex, i);
				idx = codeplugContactGetDataForNumber(mNum + 1, contactCallType, &contact);

				if (idx > 0)
				{
					codeplugUtilConvertBufToString(contact.name, nameBuf, 16); // need to convert to zero terminated string
					menuDisplayEntry(i, mNum, (char*) nameBuf);
				}
			}
		}
		break;
	case MENU_CONTACT_LIST_CONFIRM:
		codeplugUtilConvertBufToString(contactListContactData.name, nameBuf, 16);
		menuDisplayTitle(nameBuf);
		UC1701_printCentered(16, "Delete contact?",UC1701_FONT_8x16);
		UC1701_drawChoice(UC1701_CHOICE_YESNO, false);
		break;
	case MENU_CONTACT_LIST_DELETED:
		codeplugUtilConvertBufToString(contactListContactData.name, nameBuf, 16);
		menuDisplayTitle(nameBuf);
		UC1701_printCentered(16, "Contact deleted",UC1701_FONT_8x16);
		UC1701_drawChoice(UC1701_CHOICE_DISMISS, false);
		break;
	case MENU_CONTACT_LIST_TG_IN_RXGROUP:
		codeplugUtilConvertBufToString(contactListContactData.name, nameBuf, 16);
		menuDisplayTitle(nameBuf);
		UC1701_printCentered(16, "Contact used",UC1701_FONT_8x16);
		UC1701_printCentered(32, "in RX group",UC1701_FONT_8x16);
		UC1701_drawChoice(UC1701_CHOICE_DISMISS, false);
		break;
	}
	UC1701_render();
	displayLightTrigger();
}

static void handleEvent(int buttons, int keys, int events)
{
	if (KEYCHECK_LONGDOWN(keys, KEY_RED))
	{
		contactListContactIndex = 0;
		menuSystemPopAllAndDisplayRootMenu();
		return;
	}
	switch (menuContactListDisplayState)
	{
	case MENU_CONTACT_LIST_DISPLAY:
		if (KEYCHECK_PRESS(keys, KEY_DOWN))
		{
			MENU_INC(gMenusCurrentItemIndex, gMenusEndIndex);
			contactListContactIndex = codeplugContactGetDataForNumber(
					gMenusCurrentItemIndex + 1, contactCallType,
					&contactListContactData);
			updateScreen();
		}
		else if (KEYCHECK_PRESS(keys, KEY_UP))
		{
			MENU_DEC(gMenusCurrentItemIndex, gMenusEndIndex);
			contactListContactIndex = codeplugContactGetDataForNumber(
					gMenusCurrentItemIndex + 1, contactCallType,
					&contactListContactData);
			updateScreen();
		}
		else if (KEYCHECK_SHORTUP(keys, KEY_HASH))
		{
			if (contactCallType == CONTACT_CALLTYPE_TG)
			{
				contactCallType = CONTACT_CALLTYPE_PC;
			}
			else
			{
				contactCallType = CONTACT_CALLTYPE_TG;
			}
			reloadContactList();
			updateScreen();
		}
		else if (KEYCHECK_SHORTUP(keys, KEY_GREEN))
		{
			if (menuSystemGetCurrentMenuNumber() == MENU_CONTACT_QUICKLIST)
			{
				setOverrideTGorPC(contactListContactData.tgNumber, contactListContactData.callType == CONTACT_CALLTYPE_PC);
				contactListContactIndex = 0;
				menuSystemPopAllAndDisplayRootMenu();
				return;
			} else {
				menuSystemPushNewMenu(MENU_CONTACT_LIST_SUBMENU);
				return;
			}
		}
		else if (KEYCHECK_SHORTUP(keys, KEY_RED))
		{
			contactListContactIndex = 0;
			menuSystemPopPreviousMenu();
			return;
		}
		break;

	case MENU_CONTACT_LIST_CONFIRM:
		if (KEYCHECK_SHORTUP(keys, KEY_GREEN))
		{
			memset(contact.name, 0xff, 16);
			contact.tgNumber = 0;
			contact.callType = 0;
			codeplugContactSaveDataForIndex(contactListContactIndex, &contact);
			contactListContactIndex = 0;
			menuContactListTimeout = 2000;
			menuContactListDisplayState = MENU_CONTACT_LIST_DELETED;
			reloadContactList();
			updateScreen();
		}
		else if (KEYCHECK_SHORTUP(keys, KEY_RED))
		{
			menuContactListDisplayState = MENU_CONTACT_LIST_DISPLAY;
			reloadContactList();
			updateScreen();
		}
		break;

	case MENU_CONTACT_LIST_DELETED:
	case MENU_CONTACT_LIST_TG_IN_RXGROUP:
		menuContactListTimeout--;
		if ((menuContactListTimeout == 0) || KEYCHECK_SHORTUP(keys, KEY_GREEN) || KEYCHECK_SHORTUP(keys, KEY_RED))
		{
			menuContactListDisplayState = MENU_CONTACT_LIST_DISPLAY;
			reloadContactList();
		}
		updateScreen();
		break;
	}
}

enum CONTACT_LIST_QUICK_MENU_ITEMS
{
	CONTACT_LIST_QUICK_MENU_SELECT = 0,
	CONTACT_LIST_QUICK_MENU_EDIT,
	CONTACT_LIST_QUICK_MENU_DELETE,
	NUM_CONTACT_LIST_QUICK_MENU_ITEMS    // The last item in the list is used so that we automatically get a total number of items in the list
};

static void updateSubMenuScreen(void)
{
	int mNum = 0;
	char buf[17];

	UC1701_clearBuf();

	codeplugUtilConvertBufToString(contactListContactData.name, buf, 16);
	menuDisplayTitle(buf);

	for(int i = -1; i <= 1; i++)
	{
		mNum = menuGetMenuOffset(NUM_CONTACT_LIST_QUICK_MENU_ITEMS, i);

		switch(mNum)
		{
			case CONTACT_LIST_QUICK_MENU_SELECT:
				strcpy(buf, "Select TX");
				break;
			case CONTACT_LIST_QUICK_MENU_EDIT:
				strcpy(buf, "Edit Contact");
				break;
			case CONTACT_LIST_QUICK_MENU_DELETE:
				strcpy(buf, "Delete Contact");
				break;
			default:
				strcpy(buf, "");
		}

		menuDisplayEntry(i, mNum, buf);
	}

	UC1701_render();
	displayLightTrigger();
}

static void handleSubMenuEvent(int buttons, int keys, int events)
{
	if (KEYCHECK_SHORTUP(keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
	}
	else if (KEYCHECK_SHORTUP(keys, KEY_GREEN))
	{
		menuContactListOverrideState = 0;
		switch (gMenusCurrentItemIndex)
		{
		case CONTACT_LIST_QUICK_MENU_SELECT:
			setOverrideTGorPC(contactListContactData.tgNumber, contactListContactData.callType == CONTACT_CALLTYPE_PC);
			contactListContactIndex = 0;
			menuSystemPopAllAndDisplayRootMenu();
			break;
		case CONTACT_LIST_QUICK_MENU_EDIT:
			menuSystemPushNewMenu(MENU_CONTACT_DETAILS);
			break;
		case CONTACT_LIST_QUICK_MENU_DELETE:
 			if (contactListContactIndex > 0)
			{
				if (contactListContactData.callType == CONTACT_CALLTYPE_TG && codeplugContactGetRXGroup(contactListContactData.NOT_IN_CODEPLUGDATA_indexNumber)) {
					menuContactListTimeout = 2000;
					menuContactListOverrideState = MENU_CONTACT_LIST_TG_IN_RXGROUP;
				} else {
					menuContactListOverrideState = MENU_CONTACT_LIST_CONFIRM;
				}
				menuSystemPopPreviousMenu();
			}
			break;
		}
	}
	else if (KEYCHECK_PRESS(keys, KEY_DOWN))
	{
		MENU_INC(gMenusCurrentItemIndex, NUM_CONTACT_LIST_QUICK_MENU_ITEMS);
		updateSubMenuScreen();
	}
	else if (KEYCHECK_PRESS(keys, KEY_UP))
	{
		MENU_DEC(gMenusCurrentItemIndex, NUM_CONTACT_LIST_QUICK_MENU_ITEMS);
		updateSubMenuScreen();
	}
}

int menuContactListSubMenu(int buttons, int keys, int events, bool isFirstRun)
{
	if (isFirstRun)
	{
		gMenusCurrentItemIndex=0;
		updateSubMenuScreen();
		fw_init_keyboard();
	}
	else
	{
		if (events!=0 && keys!=0)
		{
			handleSubMenuEvent(buttons, keys, events);
		}
	}
	return 0;
}
