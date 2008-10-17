/** \file
 * \brief Menu Resources.
 *
 * See Copyright Notice in iup.ih
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "iup.h"

#include "iup_object.h"
#include "iup_attrib.h"
#include "iup_dialog.h"
#include "iup_str.h"
#include "iup_assert.h"
#include "iup_key.h"
#include "iup_stdcontrols.h"
#include "iup_drvinfo.h"
#include "iup_menu.h"


struct _IcontrolData 
{
  int child_id;       /* serial number used by child controls */
};

static Ihandle* iMenuGetTopMenu(Ihandle* ih)
{
  for (; ih->parent; ih = ih->parent)
    ; /* empty*/
  return ih;
}

int iupMenuGetChildId(Ihandle* ih)
{
  Ihandle* dlg = IupGetDialog(ih);
  if (dlg)
    return iupDialogGetChildId(ih);
  else
  {
    int id;
    ih = iMenuGetTopMenu(ih);
    if (!ih) return -1;
    id = ih->data->child_id;
    if (id == 0) id = 100; /* initial number */
    ih->data->child_id = id+1;
    return id;
  }
}

char* iupMenuGetChildIdStr(Ihandle* ih)
{
  Ihandle* dlg = IupGetDialog(ih);
  if (dlg)
    return iupDialogGetChildIdStr(ih);
  else
  {
    char *str = iupStrGetMemory(50);
    Ihandle* dialog = iMenuGetTopMenu(ih);
    sprintf(str, "iup-%s-%d", ih->iclass->name, dialog->data->child_id);
    return str;
  }
}

int iupMenuIsMenuBar(Ihandle* ih)
{
  if (ih->parent && ih->parent->iclass->nativetype == IUP_TYPEDIALOG)
    return 1;
  else
    return 0;
}

static void iMenuAdjustPos(int *x, int *y)
{
  int cursor_x = 0, cursor_y = 0;
  int screen_width = 0, screen_height = 0;

  if (*x == IUP_CENTER || *y == IUP_CENTER ||
      *x == IUP_RIGHT  || *y == IUP_RIGHT ||
      *x == IUP_CENTERPARENT || *y == IUP_CENTERPARENT)
    iupdrvGetScreenSize(&screen_width, &screen_height);

  if (*x == IUP_MOUSEPOS || *y == IUP_MOUSEPOS)
    iupdrvGetCursorPos(&cursor_x, &cursor_y);

  switch (*x)
  {
  case IUP_CENTER:
    *x = screen_width/2;
    break;
  case IUP_LEFT:
    *x = 0;
    break;
  case IUP_RIGHT:
    *x = screen_width;
    break;
  case IUP_MOUSEPOS:
    *x = cursor_x;
    break;
  }

  switch (*y)
  {
  case IUP_CENTER:
    *y = screen_height/2;
    break;
  case IUP_LEFT:
    *y = 0;
    break;
  case IUP_RIGHT:
    *y = screen_height;
    break;
  case IUP_MOUSEPOS:
    *y = cursor_y;
    break;
  }
}

char* iupMenuGetTitle(Ihandle* ih, const char* title)
{
  int keychar;
  char* str;

  char* key = iupAttribGetStr(ih, "KEY");
  if (!key) return (char*)title;

  keychar = iupKeyNameToCode(key);
  if (!keychar) return (char*)title;

  str = strchr(title, keychar);
  if (str)
  {
    int len = strlen(title);
    char *new_title = malloc(len+1+1);
    int pos = str-title;
    memcpy(new_title, title, pos);
    new_title[pos] = '&';
    memcpy(new_title+pos+1, title+pos, len-pos+1);
    return new_title;
  }

  return (char*)title;
}

int iupMenuPopup(Ihandle* ih, int x, int y)
{
  iMenuAdjustPos(&x, &y);
  return iupdrvMenuPopup(ih, x, y);
}


/******************************************************************/


static int iItemCreateMethod(Ihandle* ih, void** params)
{
  if (params)
  {
    if (params[0]) iupAttribStoreStr(ih, "TITLE", (char*)(params[0]));
    if (params[1]) iupAttribStoreStr(ih, "ACTION", (char*)(params[1]));
  }
  return IUP_NOERROR;
}

static int iSubmenuCreateMethod(Ihandle* ih, void** params)
{
  if (params)
  {
    if (params[0]) iupAttribStoreStr(ih, "TITLE", (char*)(params[0]));
    if (params[1]) 
    {
      Ihandle* child = (Ihandle*)(params[1]);
      if (child->iclass->nativetype == IUP_TYPEMENU)
        IupAppend(ih, child);
    }
  }
  return IUP_NOERROR;
}

static int iMenuCreateMethod(Ihandle* ih, void** params)
{
  ih->data = iupALLOCCTRLDATA();

  if (params)
  {
    Ihandle** iparams = (Ihandle**)params;
    while (*iparams) 
    {
      Ihandle* child = (Ihandle*)(*iparams);
      if (child->iclass->nativetype == IUP_TYPEMENU)
        IupAppend(ih, child);
      iparams++;
    }
  }

  return IUP_NOERROR;
}


/******************************************************************************************/


Iclass* iupSeparatorGetClass(void)
{
  Iclass* ic = iupClassNew(NULL);

  ic->name = "separator";
  ic->format = NULL;  /* no parameters */
  ic->nativetype = IUP_TYPEMENU;
  ic->childtype = IUP_CHILDNONE;
  ic->is_interactive = 0;

  /* Common */
  iupClassRegisterAttribute(ic, "WID", iupBaseGetWidAttrib, iupBaseNoSetAttrib, NULL, IUP_MAPPED, IUP_NO_INHERIT);
  iupClassRegisterAttribute(ic, "NAME", NULL, iupBaseSetNameAttrib, NULL, IUP_NOT_MAPPED, IUP_NO_INHERIT);

  iupdrvSeparatorInitClass(ic);

  return ic;
}

Iclass* iupItemGetClass(void)
{
  Iclass* ic = iupClassNew(NULL);

  ic->name = "item";
  ic->format = "SS";  /* two optional strings */
  ic->nativetype = IUP_TYPEMENU;
  ic->childtype = IUP_CHILDNONE;
  ic->is_interactive = 1;

  /* Class functions */
  ic->Create = iItemCreateMethod;

  /* Common */
  iupClassRegisterAttribute(ic, "WID", iupBaseGetWidAttrib, iupBaseNoSetAttrib, NULL, IUP_MAPPED, IUP_NO_INHERIT);
  iupClassRegisterAttribute(ic, "NAME", NULL, iupBaseSetNameAttrib, NULL, IUP_NOT_MAPPED, IUP_NO_INHERIT);

  iupdrvItemInitClass(ic);

  return ic;
}

Iclass* iupSubmenuGetClass(void)
{
  Iclass* ic = iupClassNew(NULL);

  ic->name = "submenu";
  ic->format = "SH"; /* one string and one Ihandle (both optional) */
  ic->nativetype = IUP_TYPEMENU;
  ic->childtype = IUP_CHILD_ONE;
  ic->is_interactive = 1;

  /* Class functions */
  ic->Create = iSubmenuCreateMethod;

  /* Common */
  iupClassRegisterAttribute(ic, "WID", iupBaseGetWidAttrib, iupBaseNoSetAttrib, NULL, IUP_MAPPED, IUP_NO_INHERIT);
  iupClassRegisterAttribute(ic, "NAME", NULL, iupBaseSetNameAttrib, NULL, IUP_NOT_MAPPED, IUP_NO_INHERIT);

  iupdrvSubmenuInitClass(ic);

  return ic;
}

Iclass* iupMenuGetClass(void)
{
  Iclass* ic = iupClassNew(NULL);

  ic->name = "menu";
  ic->format = "g"; /* (Ihandle**) */
  ic->nativetype = IUP_TYPEMENU;
  ic->childtype = IUP_CHILDMANY;
  ic->is_interactive = 1;

  /* Class functions */
  ic->Create = iMenuCreateMethod;

  /* Common */
  iupClassRegisterAttribute(ic, "WID", iupBaseGetWidAttrib, iupBaseNoSetAttrib, NULL, IUP_MAPPED, IUP_NO_INHERIT);
  iupClassRegisterAttribute(ic, "NAME", NULL, iupBaseSetNameAttrib, NULL, IUP_NOT_MAPPED, IUP_NO_INHERIT);

  iupdrvMenuInitClass(ic);

  return ic;
}

/************************************************************************/

Ihandle* IupItem(const char* title, const char* action)
{
  void *params[2];
  params[0] = (void*)title;
  params[1] = (void*)action;
  return IupCreatev("item", params);
}

Ihandle* IupSubmenu(const char* title, Ihandle* child)
{
  void *params[2];
  params[0] = (void*)title;
  params[1] = (void*)child;
  return IupCreatev("submenu", params);
}

Ihandle *IupMenuv(Ihandle **children)
{
  return IupCreatev("menu", (void**)children);
}

Ihandle *IupMenu(Ihandle *child, ...)
{
  Ihandle **children;
  Ihandle *ih;

  va_list arglist;
  va_start(arglist, child);
  children = (Ihandle **)iupObjectGetParamList(child, arglist);
  va_end(arglist);

  ih = IupCreatev("menu", (void**)children);
  free(children);

  return ih;
}

Ihandle* IupSeparator(void)
{
  return IupCreate("separator");
}
