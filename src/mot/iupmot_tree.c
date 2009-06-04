#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Container.h>
#include <Xm/IconG.h>
#include <Xm/ScrolledW.h>
#include <Xm/XmosP.h>
#include <Xm/Text.h>
#include <Xm/Transfer.h>
#include <Xm/DragDrop.h>
#include <X11/keysym.h>

#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>

#include "iup.h"
#include "iupcbs.h"

#include "iup_object.h"
#include "iup_childtree.h"
#include "iup_dialog.h"
#include "iup_layout.h"
#include "iup_attrib.h"
#include "iup_str.h"
#include "iup_drv.h"
#include "iup_drvinfo.h"
#include "iup_drvfont.h"
#include "iup_stdcontrols.h"
#include "iup_key.h"
#include "iup_image.h"
#include "iup_array.h"
#include "iup_tree.h"

#include "iupmot_drv.h"
#include "iupmot_color.h"


typedef struct _motTreeItemData
{
  Pixmap image, image_mask;
  Pixmap image_expanded, image_expanded_mask;
  unsigned char kind;
  void* userdata;
} motTreeItemData;


static void motTreeShowEditField(Ihandle* ih, Widget wItem);

typedef int (*motTreeNodeFunc)(Ihandle* ih, Widget wItem, void* userdata);

static int motTreeForEach(Ihandle* ih, Widget wItem, motTreeNodeFunc func, void* userdata)
{
  WidgetList wItemChildList = NULL;
  int i, numChild;

  if (!wItem)
    wItem = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

  if (!func(ih, wItem, userdata))
    return 0;

  numChild = XmContainerGetItemChildren(ih->handle, wItem, &wItemChildList);
  for (i=0; i<numChild; i++)
  {
    /* Recursively traverse child items */
    if (!motTreeForEach(ih, wItemChildList[i], func, userdata))
    {
      XtFree((char*)wItemChildList);
      return 0;
    }
  }
  if (wItemChildList) XtFree((char*)wItemChildList);

  return 1;
}

/*****************************************************************************/
/* COPYING ITEMS (Branches and its children)                                 */
/*****************************************************************************/
/* Insert the copied item in a new location. Returns the new item. */
static Widget motTreeCopyItem(Ihandle* ih, Widget wItem, Widget wParent, int pos)
{
  Widget wNewItem;
  XmString title;
  motTreeItemData *itemData;
  Pixel fgcolor, bgcolor;
  int num_args = 0;
  Arg args[30];
  Pixmap image = XmUNSPECIFIED_PIXMAP, mask = XmUNSPECIFIED_PIXMAP;
  unsigned char state;

  iupmotSetArg(args, num_args,  XmNentryParent, wParent);
  iupmotSetArg(args, num_args, XmNmarginHeight, 0);
  iupmotSetArg(args, num_args,  XmNmarginWidth, 0);
  iupmotSetArg(args, num_args,     XmNviewType, XmSMALL_ICON);
  iupmotSetArg(args, num_args, XmNnavigationType, XmTAB_GROUP);
  iupmotSetArg(args, num_args, XmNtraversalOn, True);
  iupmotSetArg(args, num_args, XmNshadowThickness, 0);

  /* Get values to copy */
  XtVaGetValues(wItem, XmNlabelString, &title,
                          XmNuserData, &itemData,
                        XmNforeground, &fgcolor,
                   XmNsmallIconPixmap, &image, 
                     XmNsmallIconMask, &mask,
                      XmNoutlineState, &state,
                                       NULL);

  iupmotSetArg(args, num_args,  XmNlabelString, title);
  iupmotSetArg(args, num_args,       XmNuserData, itemData);
  iupmotSetArg(args, num_args,   XmNforeground, fgcolor);
  iupmotSetArg(args, num_args, XmNsmallIconPixmap, image);
  iupmotSetArg(args, num_args, XmNsmallIconMask, mask);
  iupmotSetArg(args, num_args, XmNoutlineState, state);

  iupmotSetArg(args, num_args,  XmNentryParent, wParent);
  iupmotSetArg(args, num_args, XmNpositionIndex, pos);

  XtVaGetValues(ih->handle, XmNbackground, &bgcolor, NULL);
  iupmotSetArg(args, num_args,   XmNbackground, bgcolor);

  wNewItem = XtCreateManagedWidget("icon", xmIconGadgetClass, ih->handle, args, num_args);

  /* Root always expanded */
  XtVaSetValues((Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM"), XmNoutlineState, XmEXPANDED, NULL);

  XtRealizeWidget(wNewItem);

  return wNewItem;
}

static void motTreeCopyChildren(Ihandle* ih, Widget wItemSrc, Widget wItemDst)
{
  WidgetList wItemChildList = NULL;
  int i = 0;
  int numChild = XmContainerGetItemChildren(ih->handle, wItemSrc, &wItemChildList);
  while(i != numChild)
  {
    Widget wNewItem = motTreeCopyItem(ih, wItemChildList[i], wItemDst, i);  /* Use the same order they where enumerated */

    /* Recursively transfer all the items */
    motTreeCopyChildren(ih, wItemChildList[i], wNewItem);  

    /* Go to next sibling item */
    i++;
  }

  if (wItemChildList) XtFree((char*)wItemChildList);
}

/* Copies all items in a branch to a new location. Returns the new branch node. */
static Widget motTreeCopyNode(Ihandle* ih, Widget wItemSrc, Widget wItemDst)
{
  Widget wNewItem, wParent;
  motTreeItemData *itemDataDst;
  unsigned char stateDst;
  int pos;

  XtVaGetValues(wItemDst, XmNoutlineState, &stateDst, 
                          XmNuserData, &itemDataDst, 
                          NULL);

  if (itemDataDst->kind == ITREE_BRANCH && stateDst == XmEXPANDED)
  {
    /* copy as first child of expanded branch */
    wParent = wItemDst;
    pos = 0;
  }
  else
  {
    /* copy as next brother of item or collapsed branch */
    XtVaGetValues(wItemDst, XmNentryParent, &wParent, NULL);
    XtVaGetValues(wItemDst, XmNpositionIndex, &pos, NULL);
    pos++;
  }

  wNewItem = motTreeCopyItem(ih, wItemSrc, wParent, pos);

  motTreeCopyChildren(ih, wItemSrc, wNewItem);
  return wNewItem;
}

static void motTreeContainerDeselectAll(Ihandle *ih)
{
  XKeyEvent ev;

  memset(&ev, 0, sizeof(XKeyEvent));
  ev.type = KeyPress;
  ev.display = XtDisplay(ih->handle);
  ev.send_event = True;
  ev.root = RootWindow(iupmot_display, iupmot_screen);
  ev.time = clock()*CLOCKS_PER_SEC;
  ev.window = XtWindow(ih->handle);
  ev.state = ControlMask;
  ev.keycode = XK_backslash;
  ev.same_screen = True;

  XtCallActionProc(ih->handle, "ContainerDeselectAll", (XEvent*)&ev, 0, 0);
}

static void motTreeContainerSelectAll(Ihandle *ih)
{
  XKeyEvent ev;

  memset(&ev, 0, sizeof(XKeyEvent));
  ev.type = KeyPress;
  ev.display = XtDisplay(ih->handle);
  ev.send_event = True;
  ev.root = RootWindow(iupmot_display, iupmot_screen);
  ev.time = clock()*CLOCKS_PER_SEC;
  ev.window = XtWindow(ih->handle);
  ev.state = ControlMask;
  ev.keycode = XK_slash;
  ev.same_screen = True;

  XtCallActionProc(ih->handle, "ContainerSelectAll", (XEvent*)&ev, 0, 0);
}

static Widget motTreeGetLastVisibleNode(Ihandle* ih, Widget wItem)
{
  unsigned char itemState;

  XtVaGetValues(wItem, XmNoutlineState, &itemState, NULL);

  if (itemState == XmEXPANDED)
  {
    WidgetList wChildrenTree = NULL;
    int numChildren = XmContainerGetItemChildren(ih->handle, wItem, &wChildrenTree);
    if(numChildren)
      wItem = motTreeGetLastVisibleNode(ih, wChildrenTree[numChildren - 1]);
    if (wChildrenTree) XtFree((char*)wChildrenTree);
  }

  return wItem;
}

static Widget motTreeFindVisibleNodeId(Ihandle* ih, WidgetList itemList, int numItems, Widget itemNode)
{
  Widget itemChild;
  WidgetList itemChildList;
  int i = 0;
  int numChild;
  unsigned char itemState;

  while(i != numItems)
  {
    /* ID control to traverse items */
    ih->data->id_control++;   /* not the real id since it counts only the visible ones */

    /* StateID founded! */
    if(itemList[i] == itemNode)
      return itemList[i];

    /* Check whether we have child items */
    itemChildList = NULL;
    numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);
    XtVaGetValues(itemList[i], XmNoutlineState,  &itemState, NULL);

    /* The itemWidget has child and it is expanded (visible) */
    if (numChild && itemState == XmEXPANDED)
    {
      /* pass the list of children of this item */
      itemChild = motTreeFindVisibleNodeId(ih, itemChildList, numChild, itemNode);

      /* StateID founded! */
      if(itemChild == itemNode)
      {
        XtFree((char*)itemChildList);
        return itemChild;
      }
    }

    if (itemChildList) XtFree((char*)itemChildList);
    /* Go to next sibling item */
    i++;
  }

  return NULL;
}

static Widget motTreeFindVisibleNodeFromId(Ihandle* ih, WidgetList itemList, int numItems)
{
  Widget itemChild;
  WidgetList itemChildList;
  int i = 0;
  int numChild;
  unsigned char itemState;

  while(i != numItems)
  {
    /* ID control to traverse items */
    ih->data->id_control--;    /* not the real id since it counts only the visible ones */

    /* StateID founded! */
    if(ih->data->id_control < 0)
      return itemList[i];

    /* Check whether we have child items */
    itemChildList = NULL;
    numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);
    XtVaGetValues(itemList[i], XmNoutlineState,  &itemState, NULL);

    /* The itemWidget has child and it is expanded (visible) */
    if (numChild && itemState == XmEXPANDED)
    {
      /* pass the list of children of this item */
      itemChild = motTreeFindVisibleNodeFromId(ih, itemChildList, numChild);

      /* StateID founded! */
      if(ih->data->id_control < 0)
      {
        if (itemChildList) XtFree((char*)itemChildList);
        return itemChild;
      }
    }

    if (itemChildList) XtFree((char*)itemChildList);
    /* Go to next sibling item */
    i++;
  }

  return NULL;
}

static Widget motTreeGetNextVisibleNode(Ihandle* ih, Widget wRoot, Widget wItem)
{
  Widget wNext;

  ih->data->id_control = -1;
  motTreeFindVisibleNodeId(ih, &wRoot, 1, wItem);
  ih->data->id_control++;   /* more 1 visible node */

  wNext = motTreeFindVisibleNodeFromId(ih, &wRoot, 1);

  if (ih->data->id_control >= 0)
    wNext = motTreeGetLastVisibleNode(ih, wRoot);

  return wNext;
}

static Widget motTreeGetPreviousVisibleNode(Ihandle* ih, Widget wRoot, Widget wItem)
{
  ih->data->id_control = -1;
  motTreeFindVisibleNodeId(ih, &wRoot, 1, wItem);
  ih->data->id_control--;    /* less 1 visible node */

  if (ih->data->id_control < 0)
    ih->data->id_control = 0;  /* Begin of tree = Root id */

  return motTreeFindVisibleNodeFromId(ih, &wRoot, 1);
}

static void motTreeUpdateBgColor(Ihandle* ih, WidgetList itemList, int numItems, Pixel bgcolor)
{
  WidgetList itemChildList;
  int i = 0;
  int numChild;

  while(i != numItems)
  {
    XtVaSetValues(itemList[i], XmNbackground, bgcolor, NULL);

    /* Check whether we have child items */
    itemChildList = NULL;
    numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);
    if(numChild)
      motTreeUpdateBgColor(ih, itemChildList, numChild, bgcolor);
    if (itemChildList) XtFree((char*)itemChildList);

    /* Go to next sibling item */
    i++;
  }
}

static void motTreeUpdateImages(Ihandle* ih, WidgetList itemList, int numItems, int mode)
{
  motTreeItemData *itemData;
  int i = 0;

  /* called when one of the default images is changed */

  while(i != numItems)
  {
    /* Get node attributes */
    XtVaGetValues(itemList[i], XmNuserData, &itemData, NULL);
	  
    if (itemData->kind == ITREE_BRANCH)
    {
      unsigned char itemState;
      XtVaGetValues(itemList[i], XmNoutlineState,  &itemState, NULL);
      
      if (itemState == XmEXPANDED)
      {
        if (mode == ITREE_UPDATEIMAGE_EXPANDED)
        {
          XtVaSetValues(itemList[i], XmNsmallIconPixmap, (itemData->image_expanded!=XmUNSPECIFIED_PIXMAP)? itemData->image_expanded: (Pixmap)ih->data->def_image_expanded, NULL);
          XtVaSetValues(itemList[i], XmNsmallIconMask, (itemData->image_expanded_mask!=XmUNSPECIFIED_PIXMAP)? itemData->image_expanded_mask: (Pixmap)ih->data->def_image_expanded_mask, NULL);
        }
      }
      else 
      {
        if (mode == ITREE_UPDATEIMAGE_COLLAPSED)
        {
          XtVaSetValues(itemList[i], XmNsmallIconPixmap, (itemData->image!=XmUNSPECIFIED_PIXMAP)? itemData->image: (Pixmap)ih->data->def_image_collapsed, NULL);
          XtVaSetValues(itemList[i], XmNsmallIconMask, (itemData->image_mask!=XmUNSPECIFIED_PIXMAP)? itemData->image_mask: (Pixmap)ih->data->def_image_collapsed_mask, NULL);
        }
      }

      /* Recursively traverse child items */
      {
        WidgetList itemChildList;
        int numChild;
        itemChildList = NULL;
        numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);
        motTreeUpdateImages(ih, itemChildList, numChild, mode);
        if (itemChildList) XtFree((char*)itemChildList);
      }
    }
    else 
    {
      if (mode == ITREE_UPDATEIMAGE_LEAF)
      {
        XtVaSetValues(itemList[i], XmNsmallIconPixmap, (itemData->image!=XmUNSPECIFIED_PIXMAP)? itemData->image: (Pixmap)ih->data->def_image_leaf, NULL);
        XtVaSetValues(itemList[i], XmNsmallIconMask, (itemData->image_mask!=XmUNSPECIFIED_PIXMAP)? itemData->image_mask: (Pixmap)ih->data->def_image_leaf_mask, NULL);
      }
    }

    /* Go to next sibling node */
    i++;
  }
}

static int motTreeSelectFunc(Ihandle* ih, Widget wItem, int *select)
{
  int do_select = *select;
  if (do_select == -1)
  {
    unsigned char isSelected;
    XtVaGetValues(wItem, XmNvisualEmphasis, &isSelected, NULL);
    do_select = (isSelected == XmSELECTED)? 0: 1; /* toggle */
  }

  if (do_select)
    XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
  else
    XtVaSetValues(wItem, XmNvisualEmphasis, XmNOT_SELECTED, NULL);

  (void)ih;
  return 1;
}

static void motTreeInvertAllNodeMarking(Ihandle* ih)
{
  int select = -1;
  motTreeForEach(ih, NULL, (motTreeNodeFunc)motTreeSelectFunc, &select);
}

typedef struct _motTreeRange{
  Widget wItem1, wItem2;
  char inside, clear;
}motTreeRange;

static int motTreeSelectRangeFunc(Ihandle* ih, Widget wItem, motTreeRange* range)
{
  int end_range = 0;

  if (range->inside == 0) /* detect the range start */
  {
    if (range->wItem1 == wItem) range->inside=1;
    else if (range->wItem2 == wItem) range->inside=1;
  }
  else if (range->inside == 1) /* detect the range end */
  {
    if (range->wItem1 == wItem) end_range=1;
    else if (range->wItem2 == wItem) end_range=1;
  }

  if (range->inside == 1) /* if inside, select */
    XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
  else if (range->clear)  /* if outside and clear, unselect */
    XtVaSetValues(wItem, XmNvisualEmphasis, XmNOT_SELECTED, NULL);

  if (end_range || (range->inside && range->wItem1==range->wItem2))
    range->inside=-1;  /* update after selecting the node */

  (void)ih;
  return 1;
}

static void motTreeSelectRange(Ihandle* ih, Widget wItem1, Widget wItem2, int clear)
{
  motTreeRange range;
  range.wItem1 = wItem1;
  range.wItem2 = wItem2;
  range.inside = 0;
  range.clear = (char)clear;
  motTreeForEach(ih, NULL, (motTreeNodeFunc)motTreeSelectRangeFunc, &range);
}

void motTreeExpandCollapseAllNodes(Ihandle* ih, WidgetList itemList, int numItems, unsigned char itemState)
{
  WidgetList itemChildList;
  int numChild;
  int i = 0;

  while(i != numItems)
  {
    /* Check whether we have child items */
    itemChildList = NULL;
    numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);

    if(numChild)
    {
      XtVaSetValues(itemList[i], XmNoutlineState, itemState, NULL);
      motTreeExpandCollapseAllNodes(ih, itemChildList, numChild, itemState);
    }

    if (itemChildList) XtFree((char*)itemChildList);
    /* Go to next sibling item */
    i++;
  }
}

static void motTreeDestroyItemData(Widget wItem)
{
  motTreeItemData *itemData = NULL;
  XtVaGetValues(wItem, XmNuserData, &itemData, NULL);
  if (itemData)
  {
    free(itemData);
    XtVaSetValues(wItem, XmNuserData, NULL, NULL);
  }
}

static void motTreeRemoveChildren(Ihandle* ih, WidgetList itemList, int numItems, int del_userdata)
{
  WidgetList itemChildList;
  int numChild;
  int i = 0;

  while(i != numItems)
  {	  
    /* Check whether we have child items */
    itemChildList = NULL;
    numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);

    if (numChild)
      motTreeRemoveChildren(ih, itemChildList, numChild, del_userdata);

    if (del_userdata)
      motTreeDestroyItemData(itemList[i]);

    XtDestroyWidget(itemList[i]);

    if (itemChildList) XtFree((char*)itemChildList);
    /* Go to next sibling item */
    i++;
  }
}

static void motTreeRemoveNode(Ihandle* ih, Widget wItem, int del_userdata)
{
  WidgetList wChildList = NULL;
  int numChild = XmContainerGetItemChildren(ih->handle, wItem, &wChildList);
  if (numChild)
    motTreeRemoveChildren(ih, wChildList, numChild, del_userdata);
  if (del_userdata)
    motTreeDestroyItemData(wItem);
  XtDestroyWidget(wItem);
  if (wChildList) XtFree((char*)wChildList);
}

static Widget motTreeFindNodeID(Ihandle* ih, WidgetList itemList, int numItems, Widget itemNode)
{
  Widget itemChild;
  WidgetList itemChildList;
  int i = 0;
  int numChild;

  while(i != numItems)
  {
    /* ID control to traverse items */
    ih->data->id_control++;

    /* StateID founded! */
    if(itemList[i] == itemNode)
      return itemList[i];

    /* Check whether we have child items */
    itemChildList = NULL;
    numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);
    if(numChild)
    {
      /* pass the list of children of this item */
      itemChild = motTreeFindNodeID(ih, itemChildList, numChild, itemNode);

      /* StateID founded! */
      if(itemChild == itemNode)
      {
        if (itemChildList) XtFree((char*)itemChildList);
        return itemChild;
      }
    }

    if (itemChildList) XtFree((char*)itemChildList);
    /* Go to next sibling item */
    i++;
  }

  return NULL;
}

static Widget motTreeFindNodeFromID(Ihandle* ih, WidgetList itemList, int numItems)
{
  Widget itemChild;
  WidgetList itemChildList;
  int i = 0;
  int numChild;

  while(i != numItems)
  {
    /* ID control to traverse items */
    ih->data->id_control--;

    /* StateID founded! */
    if(ih->data->id_control < 0)
      return itemList[i];

    /* Check whether we have child items */
    itemChildList = NULL;
    numChild = XmContainerGetItemChildren(ih->handle, itemList[i], &itemChildList);
    if(numChild)
    {
      /* pass the list of children of this item */
      itemChild = motTreeFindNodeFromID(ih, itemChildList, numChild);

      /* StateID founded! */
      if(ih->data->id_control < 0)
      {
        if (itemChildList) XtFree((char*)itemChildList);
        return itemChild;
      }
    }

    if (itemChildList) XtFree((char*)itemChildList);
    /* Go to next sibling item */
    i++;
  }

  return NULL;
}

static int motTreeGetNodeId(Ihandle* ih, Widget wItem)
{
  Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
  ih->data->id_control = -1;
  motTreeFindNodeID(ih, &wRoot, 1, wItem);  /* use only the first element (base) of the selected objects array */
  return ih->data->id_control;
}

static Widget motTreeGetFocusNode(Ihandle* ih)
{
  Widget wItem = XmGetFocusWidget(ih->handle);
  if (wItem && XtParent(wItem) == ih->handle) /* is a node */
    return wItem;

  return (Widget)iupAttribGet(ih, "_IUPTREE_LAST_FOCUS");
}

static Widget motTreeFindNodeFromString(Ihandle* ih, const char* name_id)
{
  if (name_id[0])
  {
    Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
    iupStrToInt(name_id, &ih->data->id_control);
    return motTreeFindNodeFromID(ih, &wRoot, 1);
  }
  else
    return motTreeGetFocusNode(ih);
}

static void motTreeEnterLeaveWindowEvent(Widget w, Ihandle *ih, XEvent *evt, Boolean *cont)
{
  if (iupAttribGet(ih, "_IUPTREE_EDITFIELD"))
    return;

  /* usually when one Gadget is selected different than the previous one,
     leave/enter events are generated. But we could not find the exact condition, 
     so this is a workaround. Some leave events will be lost. */
  if (evt->type == EnterNotify)
  {
    if (iupAttribGet(ih, "_IUPTREE_IGNORE_ENTERLEAVE"))
    {
      iupAttribSetStr(ih, "_IUPTREE_IGNORE_ENTERLEAVE", NULL);
      return;
    }
  }
  else  if (evt->type == LeaveNotify)
  {
    if (iupAttribGet(ih, "_IUPTREE_IGNORE_ENTERLEAVE"))
      return;
  }

  iupmotEnterLeaveWindowEvent(w, ih, evt, cont);
}

static void motTreeFocusChangeEvent(Widget w, Ihandle *ih, XEvent *evt, Boolean *cont)
{
  unsigned char selpol;
  Widget wItem = XmGetFocusWidget(w);
  Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

  if (XtParent(wItem) == w) /* is a node */
    iupAttribSetStr(ih, "_IUPTREE_LAST_FOCUS", (char*)wItem);

  if (wItem == NULL || wItem == wRoot)
  {
    iupmotFocusChangeEvent(w, ih, evt, cont);
    return;
  }

  XtVaGetValues(w, XmNselectionPolicy, &selpol, NULL);
  if (selpol != XmSINGLE_SELECT)
    return;

  if (evt->type == FocusIn && !iupStrBoolean(IupGetGlobal("CONTROLKEY")))
  {
    XtVaSetValues(w, XmNselectedObjects,  NULL, NULL);
    XtVaSetValues(w, XmNselectedObjectCount, 0, NULL);
    XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
  }
}

void iupdrvTreeAddNode(Ihandle* ih, const char* name_id, int kind, const char* title, int add)
{
  Widget wItemPrev = motTreeFindNodeFromString(ih, name_id);
  Widget wNewItem;
  XmString itemTitle;
  motTreeItemData *itemData, *itemDataPrev;
  Pixel bgcolor, fgcolor;
  int kindPrev, num_args = 0;
  Arg args[30];

  if (!wItemPrev)
    return;

  itemData = calloc(1, sizeof(motTreeItemData));
  itemData->image = XmUNSPECIFIED_PIXMAP;
  itemData->image_expanded = XmUNSPECIFIED_PIXMAP;
  itemData->image_mask = XmUNSPECIFIED_PIXMAP;
  itemData->image_expanded_mask = XmUNSPECIFIED_PIXMAP;
  itemData->kind = (unsigned char)kind;

  itemTitle = XmStringCreateLocalized((String)title);

  /* Get default colors */
  XtVaGetValues(ih->handle, XmNforeground, &fgcolor, NULL);
  XtVaGetValues(ih->handle, XmNbackground, &bgcolor, NULL);

  /* Get the kind of previous item */
  XtVaGetValues(wItemPrev, XmNuserData, &itemDataPrev, NULL);
  kindPrev = itemDataPrev->kind;

  if (kindPrev == ITREE_BRANCH && add)
  {
    /* wItemPrev is parent of the new item (firstchild of it) */
    iupmotSetArg(args, num_args,   XmNentryParent, wItemPrev);
    iupmotSetArg(args, num_args, XmNpositionIndex, 0);
  }
  else
  {
    /* wItemPrev is sibling of the new item (set its parent to the new item) */
    Widget wItemParent;
    int pos;

    XtVaGetValues(wItemPrev, XmNentryParent, &wItemParent, NULL);
    XtVaGetValues(wItemPrev, XmNpositionIndex, &pos, NULL);

    iupmotSetArg(args, num_args, XmNentryParent, wItemParent);
    iupmotSetArg(args, num_args, XmNpositionIndex, pos+1);
  }

  iupmotSetArg(args, num_args,       XmNuserData, itemData);
  iupmotSetArg(args, num_args,   XmNforeground, fgcolor);
  iupmotSetArg(args, num_args,   XmNbackground, bgcolor);
  iupmotSetArg(args, num_args, XmNmarginHeight, 0);
  iupmotSetArg(args, num_args,  XmNmarginWidth, 0);
  iupmotSetArg(args, num_args,     XmNviewType, XmSMALL_ICON);
  iupmotSetArg(args, num_args, XmNnavigationType, XmTAB_GROUP);
  iupmotSetArg(args, num_args, XmNtraversalOn, True);
  iupmotSetArg(args, num_args, XmNshadowThickness, 0);
  iupmotSetArg(args, num_args, XmNlabelString, itemTitle);

  if (kind == ITREE_BRANCH)
  {
    if (ih->data->add_expanded)
    {
      iupmotSetArg(args, num_args, XmNsmallIconPixmap, ih->data->def_image_expanded);
      iupmotSetArg(args, num_args, XmNsmallIconMask, ih->data->def_image_expanded_mask);
    }
    else
    {
      iupmotSetArg(args, num_args, XmNsmallIconPixmap, ih->data->def_image_collapsed);
      iupmotSetArg(args, num_args, XmNsmallIconMask, ih->data->def_image_collapsed_mask);
    }
  }
  else
  {
    iupmotSetArg(args, num_args, XmNsmallIconPixmap, ih->data->def_image_leaf);
    iupmotSetArg(args, num_args, XmNsmallIconMask, ih->data->def_image_leaf_mask);
  }


  wNewItem = XtCreateManagedWidget("icon", xmIconGadgetClass, ih->handle, args, num_args);

  if (kind == ITREE_BRANCH)
  {
    if (ih->data->add_expanded)
    {
      iupAttribSetStr(ih, "_IUP_IGNORE_BRANCHOPEN", "1");
      XtVaSetValues(wNewItem, XmNoutlineState, XmEXPANDED, NULL);
    }
    else
      XtVaSetValues(wNewItem, XmNoutlineState, XmCOLLAPSED, NULL);
  }

  /* Root always expanded */
  XtVaSetValues((Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM"), XmNoutlineState, XmEXPANDED, NULL);

  XtRealizeWidget(wNewItem);
  XmStringFree(itemTitle);
}

static void motTreeAddRootNode(Ihandle* ih)
{
  Widget wRootItem;
  motTreeItemData *itemData;
  Pixel bgcolor, fgcolor;
  int num_args = 0;
  Arg args[30];

  itemData = calloc(1, sizeof(motTreeItemData));
  itemData->image = XmUNSPECIFIED_PIXMAP;
  itemData->image_expanded = XmUNSPECIFIED_PIXMAP;
  itemData->image_mask = XmUNSPECIFIED_PIXMAP;
  itemData->image_expanded_mask = XmUNSPECIFIED_PIXMAP;
  itemData->kind = ITREE_BRANCH;

  /* Get default foreground color */
  XtVaGetValues(ih->handle, XmNforeground, &fgcolor, NULL);
  XtVaGetValues(ih->handle, XmNbackground, &bgcolor, NULL);

  iupmotSetArg(args, num_args,  XmNentryParent, NULL);
  iupmotSetArg(args, num_args,       XmNuserData, itemData);
  iupmotSetArg(args, num_args,   XmNforeground, fgcolor);
  iupmotSetArg(args, num_args,   XmNbackground, bgcolor);
  iupmotSetArg(args, num_args, XmNoutlineState, XmEXPANDED);
  iupmotSetArg(args, num_args, XmNmarginHeight, 0);
  iupmotSetArg(args, num_args,  XmNmarginWidth, 0);
  iupmotSetArg(args, num_args,     XmNviewType, XmSMALL_ICON);
  iupmotSetArg(args, num_args, XmNnavigationType, XmTAB_GROUP);
  iupmotSetArg(args, num_args, XmNtraversalOn, True);
  iupmotSetArg(args, num_args, XmNshadowThickness, 0);
  iupmotSetArg(args, num_args, XmNsmallIconPixmap, ih->data->def_image_expanded);
  iupmotSetArg(args, num_args, XmNsmallIconMask, ih->data->def_image_expanded_mask);

  wRootItem = XtCreateManagedWidget("icon", xmIconGadgetClass, ih->handle, args, num_args);

  /* Select the new item */
  XtVaSetValues(wRootItem, XmNvisualEmphasis, XmSELECTED, NULL);

  XtRealizeWidget(wRootItem);

  /* Save the root node for later use */
  iupAttribSetStr(ih, "_IUPTREE_ROOTITEM", (char*)wRootItem);

  /* MarkStart node */
  iupAttribSetStr(ih, "_IUPTREE_MARKSTART_NODE", (char*)wRootItem);
}

/*****************************************************************************/

static int motTreeSetImageExpandedAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  motTreeItemData *itemData;
  unsigned char itemState;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)
    return 0;

  XtVaGetValues(wItem, XmNuserData, &itemData, NULL);
  XtVaGetValues(wItem, XmNoutlineState, &itemState, NULL);
  itemData->image_expanded = (Pixmap)iupImageGetImage(value, ih, 0, "IMAGEEXPANDED");
  if (!itemData->image_expanded)
  {
    itemData->image_expanded = XmUNSPECIFIED_PIXMAP;
    itemData->image_expanded_mask = XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    itemData->image_expanded_mask = (Pixmap)iupImageGetMask(value);
    if (!itemData->image_expanded_mask) itemData->image_expanded_mask = XmUNSPECIFIED_PIXMAP;
  }

  if (itemData->kind == ITREE_BRANCH && itemState == XmEXPANDED)
  {
    if (itemData->image_expanded == XmUNSPECIFIED_PIXMAP)
      XtVaSetValues(wItem, XmNsmallIconPixmap, (Pixmap)ih->data->def_image_expanded, 
                           XmNsmallIconMask, (Pixmap)ih->data->def_image_expanded_mask, 
                           NULL);
    else
      XtVaSetValues(wItem, XmNsmallIconPixmap, itemData->image_expanded, 
                           XmNsmallIconMask, itemData->image_expanded_mask, 
                           NULL);
  }

  return 1;
}

static int motTreeSetImageAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  motTreeItemData *itemData;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  XtVaGetValues(wItem, XmNuserData, &itemData, NULL);
  itemData->image = (Pixmap)iupImageGetImage(value, ih, 0, "IMAGE");
  if (!itemData->image) 
  {
    itemData->image = XmUNSPECIFIED_PIXMAP;
    itemData->image_mask = XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    itemData->image_mask = (Pixmap)iupImageGetMask(value);
    if (!itemData->image_mask) itemData->image_mask = XmUNSPECIFIED_PIXMAP;
  }

  if (itemData->kind == ITREE_BRANCH)
  {
    unsigned char itemState;
    XtVaGetValues(wItem, XmNoutlineState, &itemState, NULL);
    if (itemState == XmCOLLAPSED)
    {
      if (itemData->image == XmUNSPECIFIED_PIXMAP)
        XtVaSetValues(wItem, XmNsmallIconPixmap, (Pixmap)ih->data->def_image_collapsed, 
                             XmNsmallIconMask, (Pixmap)ih->data->def_image_collapsed_mask, 
                             NULL);
      else
        XtVaSetValues(wItem, XmNsmallIconPixmap, itemData->image, 
                             XmNsmallIconMask, itemData->image_mask, 
                             NULL);
    }
  }
  else
  {
    if (itemData->image == XmUNSPECIFIED_PIXMAP)
      XtVaSetValues(wItem, XmNsmallIconPixmap, (Pixmap)ih->data->def_image_leaf, 
                           XmNsmallIconMask, (Pixmap)ih->data->def_image_leaf_mask, 
                           NULL);
    else
      XtVaSetValues(wItem, XmNsmallIconPixmap, itemData->image, 
                           XmNsmallIconMask, itemData->image_mask, 
                           NULL);
  }

  return 1;
}

static int motTreeSetImageBranchExpandedAttrib(Ihandle* ih, const char* value)
{
  Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
  ih->data->def_image_expanded = iupImageGetImage(value, ih, 0, "IMAGEBRANCHEXPANDED");
  if (!ih->data->def_image_expanded) 
  {
    ih->data->def_image_expanded = (void*)XmUNSPECIFIED_PIXMAP;
    ih->data->def_image_expanded_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    ih->data->def_image_expanded_mask = iupImageGetMask(value);
    if (!ih->data->def_image_expanded_mask) ih->data->def_image_expanded_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
    
  /* Update all images, starting at root node */
  motTreeUpdateImages(ih, &wRoot, 1, ITREE_UPDATEIMAGE_EXPANDED);

  return 1;
}

static int motTreeSetImageBranchCollapsedAttrib(Ihandle* ih, const char* value)
{
  Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
  ih->data->def_image_collapsed = iupImageGetImage(value, ih, 0, "IMAGEBRANCHCOLLAPSED");
  if (!ih->data->def_image_collapsed) 
  {
    ih->data->def_image_collapsed = (void*)XmUNSPECIFIED_PIXMAP;
    ih->data->def_image_collapsed_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    ih->data->def_image_collapsed_mask = iupImageGetMask(value);
    if (!ih->data->def_image_collapsed_mask) ih->data->def_image_collapsed_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
   
  /* Update all images, starting at root node */
  motTreeUpdateImages(ih, &wRoot, 1, ITREE_UPDATEIMAGE_COLLAPSED);

  return 1;
}

static int motTreeSetImageLeafAttrib(Ihandle* ih, const char* value)
{
  Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
  ih->data->def_image_leaf = iupImageGetImage(value, ih, 0, "IMAGELEAF");
  if (!ih->data->def_image_leaf) 
  {
    ih->data->def_image_leaf = (void*)XmUNSPECIFIED_PIXMAP;
    ih->data->def_image_leaf_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    ih->data->def_image_leaf_mask = iupImageGetMask(value);
    if (!ih->data->def_image_leaf_mask) ih->data->def_image_leaf_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
    
  /* Update all images, starting at root node */
  motTreeUpdateImages(ih, &wRoot, 1, ITREE_UPDATEIMAGE_LEAF);

  return 1;
}

static char* motTreeGetStateAttrib(Ihandle* ih, const char* name_id)
{
  int hasChildren;
  unsigned char itemState;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  XtVaGetValues(wItem, XmNnumChildren, &hasChildren, NULL);
  XtVaGetValues(wItem, XmNoutlineState, &itemState, NULL);
  
  if (hasChildren)
  {
    if(itemState == XmEXPANDED)
      return "EXPANDED";
    else
      return "COLLAPSED";
  }

  return NULL;
}

static int motTreeSetStateAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  if (iupStrEqualNoCase(value, "EXPANDED"))
    XtVaSetValues(wItem, XmNoutlineState, XmEXPANDED, NULL);
  else 
    XtVaSetValues(wItem, XmNoutlineState, XmCOLLAPSED, NULL);

  return 0;
}

static char* motTreeGetColorAttrib(Ihandle* ih, const char* name_id)
{
  unsigned char r, g, b;
  Pixel color;
  char* str;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);  
  if (!wItem)  
    return NULL;

  XtVaGetValues(wItem, XmNforeground, &color, NULL); 
  iupmotColorGetRGB(color, &r, &g, &b);

  str = iupStrGetMemory(20);
  sprintf(str, "%d %d %d", (int)r, (int)g, (int)b);
  return str;
}

static int motTreeSetColorAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  Pixel color;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);  
  if (!wItem)  
    return 0;

  color = iupmotColorGetPixelStr(value);
  if (color != (Pixel)-1)
    XtVaSetValues(wItem, XmNforeground, color, NULL);
  return 0; 
}

static char* motTreeGetDepthAttrib(Ihandle* ih, const char* name_id)
{
  Widget wRoot;
  int dep = 0;
  char* depth;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);  
  if (!wItem)  
    return NULL;

  wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

  while((wRoot != wItem) && (wItem != NULL))
  {
    XtVaGetValues(wItem, XmNentryParent, &wItem, NULL);
    dep++;
  }

  depth = iupStrGetMemory(10);
  sprintf(depth, "%d", dep);
  return depth;
}

static int motTreeSetMoveNodeAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  Widget wItemDst, wParent;
  Widget wItemSrc = motTreeFindNodeFromString(ih, name_id);
  if (!wItemSrc)
    return 0;
  wItemDst = motTreeFindNodeFromString(ih, value);
  if (!wItemDst)
    return 0;

  /* If Drag item is an ancestor of Drop item then return */
  wParent = wItemDst;
  while(wParent)
  {
    XtVaGetValues(wParent, XmNentryParent, &wParent, NULL);
    if (wParent == wItemSrc)
      return 0;
  }

  /* Copying the node and its children to the new position */
  motTreeCopyNode(ih, wItemSrc, wItemDst);

  /* Deleting the node (and its children) inserted into the old position */
  motTreeRemoveNode(ih, wItemSrc, 0);   /* do not delete the user data, we copy the references in CopyBranch */

  return 0;
}

static char* motTreeGetParentAttrib(Ihandle* ih, const char* name_id)
{
  Widget wItemParent;
  char* str;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return NULL;

  /* get the parent item */
  XtVaGetValues(wItem, XmNentryParent, &wItemParent, NULL);
  if (!wItemParent)
    return NULL;

  str = iupStrGetMemory(10);
  sprintf(str, "%d", motTreeGetNodeId(ih, wItemParent));
  return str;
}

static char* motTreeGetChildCountAttrib(Ihandle* ih, const char* name_id)
{
  char* str;
  WidgetList wList = NULL;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return NULL;

  str = iupStrGetMemory(10);
  sprintf(str, "%d", XmContainerGetItemChildren(ih->handle, wItem, &wList));
  if (wList) XtFree((char*)wList);
  return str;
}

static int motTreeCount(Ihandle* ih, Widget wItem)
{
  WidgetList wList = NULL;
  int i, count = 0;
  int childCount = XmContainerGetItemChildren(ih->handle, wItem, &wList);
  count++;
  for (i=0; i<childCount; i++)
    count += motTreeCount(ih, wList[i]);
  if (wList) XtFree((char*)wList);
  return count;
}

static char* motTreeGetCountAttrib(Ihandle* ih)
{
  char* str = iupStrGetMemory(10);
  Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
  sprintf(str, "%d", motTreeCount(ih, wRoot));
  return str;
}

static char* motTreeGetKindAttrib(Ihandle* ih, const char* name_id)
{
  motTreeItemData *itemData;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return NULL;

  XtVaGetValues(wItem, XmNuserData, &itemData, NULL);

  if(itemData->kind == ITREE_BRANCH)
    return "BRANCH";
  else
    return "LEAF";
}

static char* motTreeGetValueAttrib(Ihandle* ih)
{
  char* str;
  Widget wItem = motTreeGetFocusNode(ih);
  if(!wItem)
    return NULL;

  str = iupStrGetMemory(10);
  sprintf(str, "%d", motTreeGetNodeId(ih, wItem));
  return str;
}

static int motTreeSetMarkAttrib(Ihandle* ih, const char* value)
{
  if (ih->data->mark_mode==ITREE_MARK_SINGLE)
    return 0;

  if(iupStrEqualNoCase(value, "CLEARALL"))
    motTreeContainerDeselectAll(ih);
  else if(iupStrEqualNoCase(value, "MARKALL"))
    motTreeContainerSelectAll(ih);
  else if(iupStrEqualNoCase(value, "INVERTALL")) /* INVERTALL *MUST* appear before INVERT, or else INVERTALL will never be called. */
    motTreeInvertAllNodeMarking(ih);
  else if(iupStrEqualPartial(value, "INVERT"))
  {
    unsigned char isSelected;
    Widget wItem = motTreeFindNodeFromString(ih, &value[strlen("INVERT")]);
    if (!wItem)  
      return 0;

    XtVaGetValues(wItem, XmNvisualEmphasis, &isSelected, NULL);
    if (isSelected == XmSELECTED)
      XtVaSetValues(wItem, XmNvisualEmphasis, XmNOT_SELECTED, NULL);
    else
      XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
  }
  else if(iupStrEqualNoCase(value, "BLOCK"))
  {
    Widget wItem = (Widget)iupAttribGet(ih, "_IUPTREE_MARKSTART_NODE");
    Widget wFocusItem = motTreeGetFocusNode(ih);
    if(!wFocusItem || !wItem)
      return 0;
    motTreeSelectRange(ih, wFocusItem, wItem, 0);
  }
  else
  {
    Widget wItem1, wItem2;
    char str1[50], str2[50];
    if (iupStrToStrStr(value, str1, str2, '-')!=2)
      return 0;

    wItem1 = motTreeFindNodeFromString(ih, str1);
    if (!wItem1)  
      return 0;
    wItem2 = motTreeFindNodeFromString(ih, str2);
    if (!wItem2)  
      return 0;

    motTreeSelectRange(ih, wItem1, wItem2, 0);
  }

  return 1;
} 

static int motTreeSetValueAttrib(Ihandle* ih, const char* value)
{
  Widget wRoot, wItem;

  if (motTreeSetMarkAttrib(ih, value))
    return 0;

  wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

  if (iupStrEqualNoCase(value, "ROOT"))
    wItem = wRoot;
  else if(iupStrEqualNoCase(value, "LAST"))
    wItem = motTreeGetLastVisibleNode(ih, wRoot);
  else if(iupStrEqualNoCase(value, "PGUP"))
  {
    Widget wItemFocus = motTreeGetFocusNode(ih);
    if(!wItemFocus)
      return 0;

    ih->data->id_control = -1;
    motTreeFindVisibleNodeId(ih, &wRoot, 1, wItemFocus);
    ih->data->id_control -= 10;  /* less 10 visible nodes */

    if(ih->data->id_control < 0)
      ih->data->id_control = 0;  /* Begin of tree = Root id */

    wItem = motTreeFindVisibleNodeFromId(ih, &wRoot, 1);
  }
  else if(iupStrEqualNoCase(value, "PGDN"))
  {
    Widget wNext, wItemFocus;

    wItemFocus = motTreeGetFocusNode(ih);
    if(!wItemFocus)
      return 0;

    ih->data->id_control = -1;
    motTreeFindVisibleNodeId(ih, &wRoot, 1, wItemFocus);
    ih->data->id_control += 10;  /* more 10 visible nodes */

    wNext = motTreeFindVisibleNodeFromId(ih, &wRoot, 1);

    if (ih->data->id_control >= 0)
      wNext = motTreeGetLastVisibleNode(ih, wRoot);
 
    wItem = wNext;
  }
  else if(iupStrEqualNoCase(value, "NEXT"))
  {
    Widget wItemFocus = motTreeGetFocusNode(ih);
    if (!wItemFocus)
      return 0;

    wItem = motTreeGetNextVisibleNode(ih, wRoot, wItemFocus);
  }
  else if(iupStrEqualNoCase(value, "PREVIOUS"))
  {
    Widget wItemFocus = motTreeGetFocusNode(ih);
    if(!wItemFocus)
      return 0;

    wItem = motTreeGetPreviousVisibleNode(ih, wRoot, wItemFocus);
  }
  else
    wItem = motTreeFindNodeFromString(ih, value);

  if (!wItem)  
    return 0;

  /* select */
  if (ih->data->mark_mode==ITREE_MARK_SINGLE)
  {
    /* clear previous selection */
    XtVaSetValues(ih->handle, XmNselectedObjects,  NULL, NULL);
    XtVaSetValues(ih->handle, XmNselectedObjectCount, 0, NULL);

    XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
  }

  /* set focus (will scroll to visible) */
  XmProcessTraversal(wItem, XmTRAVERSE_CURRENT);

  iupAttribSetInt(ih, "_IUPTREE_OLDVALUE", motTreeGetNodeId(ih, wItem));

  return 0;
} 

static int motTreeSetMarkStartAttrib(Ihandle* ih, const char* name_id)
{
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  iupAttribSetStr(ih, "_IUPTREE_MARKSTART_NODE", (char*)wItem);

  return 1;
}

static char* motTreeGetMarkedAttrib(Ihandle* ih, const char* name_id)
{
  unsigned char isSelected;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return NULL;

  XtVaGetValues(wItem, XmNvisualEmphasis, &isSelected, NULL);

  if(isSelected == XmSELECTED)
    return "YES";
  else
    return "NO";
}

static int motTreeSetMarkedAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  if (iupStrBoolean(value))
    XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
  else
    XtVaSetValues(wItem, XmNvisualEmphasis, XmNOT_SELECTED, NULL);

  return 0;
}

static char* motTreeGetTitle(Widget wItem)
{
  char *title;
  XmString itemTitle;
  XtVaGetValues(wItem, XmNlabelString, &itemTitle, NULL);
  title = iupmotConvertString(itemTitle);
  XmStringFree(itemTitle);
  return title;
}

static char* motTreeGetTitleAttrib(Ihandle* ih, const char* name_id)
{
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return NULL;
  return motTreeGetTitle(wItem);
}

static int motTreeSetTitleAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  iupmotSetString(wItem, XmNlabelString, value);

  return 0;
}

static int motTreeSetTitleFontAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  XmFontList fontlist = NULL;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  if (value)
  {
    char attr[20];
    sprintf(attr, "TITLEFOUNDRY%s", name_id);
    fontlist = iupmotGetFontList(iupAttribGet(ih, attr), value);
  }
  XtVaSetValues(wItem, XmNrenderTable, fontlist, NULL);

  return 0;
}

static char* motTreeGetTitleFontAttrib(Ihandle* ih, const char* name_id)
{
  XmFontList fontlist;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return NULL;

  XtVaGetValues(wItem, XmNrenderTable, &fontlist, NULL);
  return iupmotFindFontList(fontlist);
}

static char* motTreeGetUserDataAttrib(Ihandle* ih, const char* name_id)
{
  motTreeItemData *itemData;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return NULL;

  XtVaGetValues(wItem, XmNuserData, &itemData, NULL);

  return itemData->userdata;
}

static int motTreeSetUserDataAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  motTreeItemData *itemData;
  Widget wItem = motTreeFindNodeFromString(ih, name_id);
  if (!wItem)  
    return 0;

  XtVaGetValues(wItem, XmNuserData, &itemData, NULL);
  itemData->userdata = (void*)value;

  return 0;
}

static int motTreeSetRenameAttrib(Ihandle* ih, const char* value)
{  
  if (ih->data->show_rename)
  {
    IFni cbShowRename = (IFni)IupGetCallback(ih, "SHOWRENAME_CB");
    Widget wItemFocus = motTreeGetFocusNode(ih);
    if (cbShowRename)
      cbShowRename(ih, motTreeGetNodeId(ih, wItemFocus));
    motTreeShowEditField(ih, wItemFocus);
  }
  else
  {
    IFnis cbRenameNode = (IFnis)IupGetCallback(ih, "RENAMENODE_CB");
    if (cbRenameNode)
    {
      Widget wItemFocus = motTreeGetFocusNode(ih);
      cbRenameNode(ih, motTreeGetNodeId(ih, wItemFocus), motTreeGetTitle(wItemFocus));  
    }
  }

  (void)value;
  return 0;
}

static int motTreeSetDelNodeAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  if(iupStrEqualNoCase(value, "SELECTED"))  /* selectec here means the specified one */
  {
    Widget wItem = motTreeFindNodeFromString(ih, name_id);
    Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

    /* the root node can't be deleted */
    if(!wItem || wItem == wRoot)  /* root is the unique child */
      return 0;

    /* deleting the specified node (and it's children) */
    motTreeRemoveNode(ih, wItem, 1);
  }
  else if(iupStrEqualNoCase(value, "CHILDREN"))  /* children of the specified one */
  {
    Widget wItem = motTreeFindNodeFromString(ih, name_id);
    Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

    /* the root node can't be deleted */
    if(!wItem || wItem == wRoot)  /* root is the unique child */
      return 0;

    {
      /* deleting the selected node's children only */
      WidgetList wItemList = NULL;
      int numChild = XmContainerGetItemChildren(ih->handle, wItem, &wItemList);
      if(numChild)
        motTreeRemoveChildren(ih, wItemList, numChild, 1);
      if (wItemList) XtFree((char*)wItemList);
    }
  }
  else if(iupStrEqualNoCase(value, "MARKED"))  /* Delete the array of marked nodes */
  {
    WidgetList wSelectedItemList = NULL;
    Widget wRoot;
    int countItems, i;

    XtVaGetValues(ih->handle, XmNselectedObjects, &wSelectedItemList,
                              XmNselectedObjectCount, &countItems, NULL);

    wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

    for(i = 0; i < countItems; i++)
    {
      int ok = XmIsIconGadget(wSelectedItemList[i]);
      if ((wSelectedItemList[i] != wRoot) && ok)  /* the root node can't be deleted */
        motTreeRemoveNode(ih, wSelectedItemList[i], 1);
    }
  }

  return 0;
}

static char* motTreeGetIndentationAttrib(Ihandle* ih)
{
  char* str = iupStrGetMemory(255);
  Dimension indent;
  XtVaGetValues(ih->handle, XmNoutlineIndentation, &indent, NULL);
  sprintf(str, "%d", (int)indent);
  return str;
}

static int motTreeSetIndentationAttrib(Ihandle* ih, const char* value)
{
  int indent;
  if (iupStrToInt(value, &indent))
    XtVaSetValues(ih->handle, XmNoutlineIndentation, (Dimension)indent, NULL);
  return 0;
}

static int motTreeSetExpandAllAttrib(Ihandle* ih, const char* value)
{
  Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

  if (iupStrBoolean(value))
    motTreeExpandCollapseAllNodes(ih, &wRoot, 1, XmEXPANDED);
  else
  {
    motTreeExpandCollapseAllNodes(ih, &wRoot, 1, XmCOLLAPSED);

    /* The root node is always expanded */
    XtVaSetValues((Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM"), XmNoutlineState, XmEXPANDED, NULL);
  }

  return 0;
}

static int motTreeSetBgColorAttrib(Ihandle* ih, const char* value)
{
  Widget sb_win = (Widget)iupAttribGet(ih, "_IUP_EXTRAPARENT");
  Pixel color;

  /* ignore given value for the scrollbars, must use only from parent */
  char* parent_value = iupBaseNativeParentGetBgColor(ih);

  color = iupmotColorGetPixelStr(parent_value);
  if (color != (Pixel)-1)
  {
    Widget sb = NULL;

    iupmotSetBgColor(sb_win, color);

    XtVaGetValues(sb_win, XmNverticalScrollBar, &sb, NULL);
    if (sb) iupmotSetBgColor(sb, color);

    XtVaGetValues(sb_win, XmNhorizontalScrollBar, &sb, NULL);
    if (sb) iupmotSetBgColor(sb, color);
  }

  color = iupmotColorGetPixelStr(value);
  if (color != (Pixel)-1)
  {
    Widget wRoot;
    Widget clipwin = NULL;

    XtVaGetValues(sb_win, XmNclipWindow, &clipwin, NULL);
    if (clipwin) iupmotSetBgColor(clipwin, color);

    wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
    
    /* Update all children, starting at root node */
    motTreeUpdateBgColor(ih, &wRoot, 1, color);
  }

  iupdrvBaseSetBgColorAttrib(ih, value);   /* use given value for contents */

  /* update internal image cache */
  iupTreeUpdateImages(ih);

  return 1;
}

static int motTreeSetFgColorAttrib(Ihandle* ih, const char* value)
{
  Pixel color = iupmotColorGetPixelStr(value);
  if (color != (Pixel)-1)
    XtVaSetValues(ih->handle, XmNforeground, color, NULL);

  return 1;
}

void iupdrvTreeUpdateMarkMode(Ihandle *ih)
{
  XtVaSetValues(ih->handle, XmNselectionPolicy, (ih->data->mark_mode==ITREE_MARK_SINGLE)? XmSINGLE_SELECT: XmEXTENDED_SELECT, NULL);
}

/************************************************************************************************/


static void motTreeSetRenameCaretPos(Widget cbEdit, const char* value)
{
  int pos = 1;

  if (iupStrToInt(value, &pos))
  {
    if (pos < 1) pos = 1;
    pos--; /* IUP starts at 1 */

    XtVaSetValues(cbEdit, XmNcursorPosition, pos, NULL);
  }
}

static void motTreeSetRenameSelectionPos(Widget cbEdit, const char* value)
{
  int start = 1, end = 1;

  if (iupStrToIntInt(value, &start, &end, ':') != 2) 
    return;

  if(start < 1 || end < 1) 
    return;

  start--; /* IUP starts at 1 */
  end--;

  XmTextSetSelection(cbEdit, start, end, CurrentTime);
}

/*****************************************************************************/

static int motTreeCallBranchCloseCb(Ihandle* ih, Widget wItem)
{
  IFni cbBranchClose = (IFni)IupGetCallback(ih, "BRANCHCLOSE_CB");

  if(cbBranchClose)
    return cbBranchClose(ih, motTreeGetNodeId(ih, wItem));

  return IUP_DEFAULT;
}

static int motTreeCallBranchOpenCb(Ihandle* ih, Widget wItem)
{
  IFni cbBranchOpen;
  
  if (iupAttribGet(ih, "_IUP_IGNORE_BRANCHOPEN"))
  {
    iupAttribSetStr(ih, "_IUP_IGNORE_BRANCHOPEN", NULL);
    return IUP_DEFAULT;
  }

  cbBranchOpen = (IFni)IupGetCallback(ih, "BRANCHOPEN_CB");
  if (cbBranchOpen)
    return cbBranchOpen(ih, motTreeGetNodeId(ih, wItem));

  return IUP_DEFAULT;
}

static void motTreeCallMultiSelectionCb(Ihandle* ih)
{
  IFnIi cbMulti = (IFnIi)IupGetCallback(ih, "MULTISELECTION_CB");
  IFnii cbSelec = (IFnii)IupGetCallback(ih, "SELECTION_CB");
  WidgetList wSelectedItemList = NULL;
  Widget wRoot;
  int countItems;

  wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");

  XtVaGetValues(ih->handle, XmNselectedObjects, &wSelectedItemList,
                        XmNselectedObjectCount, &countItems, NULL);
  if (countItems == 0)
    return;

  if (cbMulti || cbSelec)
  {
    int* id_rowItem = malloc(sizeof(int) * countItems);
    int i = 0;

    for(i = 0; i < countItems; i++)
      id_rowItem[i] = motTreeGetNodeId(ih, wSelectedItemList[i]);

    if (cbMulti)
      cbMulti(ih, id_rowItem, countItems);
    else
    {
      for (i=0; i<countItems; i++)
        cbSelec(ih, id_rowItem[i], 1);
    }

    free(id_rowItem);
  }
}

static int motTreeConvertXYToPos(Ihandle* ih, int x, int y)
{
  Widget wItem = XmObjectAtPoint(ih->handle, (Position)x, (Position)y);
  if (wItem)
    return motTreeGetNodeId(ih, wItem);
  return -1;
}

static void motTreeCallRightClickCb(Ihandle* ih, int x, int y)
{
  IFni cbRightClick  = (IFni)IupGetCallback(ih, "RIGHTCLICK_CB");
  if (cbRightClick)
  {
    int id = motTreeConvertXYToPos(ih, x, y);
    if (id != -1)
      cbRightClick(ih, id);
  }    
}

static void motTreeCallRenameCb(Ihandle* ih)
{
  IFnis cbRename;
  Widget wItem, wEdit;
  int ignore = 0;
  String title = NULL;

  wItem = (Widget)iupAttribGet(ih, "_IUPTREE_SELECTED");
  wEdit = (Widget)iupAttribGet(ih, "_IUPTREE_EDITFIELD");

  XtVaGetValues((Widget)iupAttribGet(ih, "_IUPTREE_EDITFIELD"), XmNvalue, &title, NULL);

  cbRename = (IFnis)IupGetCallback(ih, "RENAME_CB");
  if (cbRename)
  {
    if (cbRename(ih, motTreeGetNodeId(ih, wItem), title) == IUP_IGNORE)
      ignore = 1;
  }

  if (!ignore)
    iupmotSetString(wItem, XmNlabelString, title);

  XtDestroyWidget(wEdit);
  XmProcessTraversal(wItem, XmTRAVERSE_CURRENT);

  iupAttribSetStr(ih, "_IUPTREE_EDITFIELD", NULL);
  iupAttribSetStr(ih, "_IUPTREE_SELECTED",  NULL);
}

static int motTreeCallDragDropCb(Ihandle* ih, Widget wItemDrag, Widget wItemDrop)
{
  IFniiii cbDragDrop = (IFniiii)IupGetCallback(ih, "DRAGDROP_CB");
  if (cbDragDrop)
  {
    int drag_id = motTreeGetNodeId(ih, wItemDrag);
    int drop_id = motTreeGetNodeId(ih, wItemDrop);
    int   isshift = 0;
    int iscontrol = 0;
    char key[5];
    iupdrvGetKeyState(key);
    if (key[0] == 'S')
      isshift = 1;
    if (key[1] == 'C')
      iscontrol = 1;

    return cbDragDrop(ih, drag_id, drop_id, isshift, iscontrol);
  }

  return IUP_CONTINUE; /* allow to move by default if callback not defined */
}

static void motTreeEditFocusChangeEvent(Widget w, Ihandle *ih, XEvent *evt, Boolean *cont)
{
  if (evt->type == FocusOut)
    motTreeCallRenameCb(ih);

  (void)cont;
  (void)w;
}

static void motTreeEditKeyPressEvent(Widget w, Ihandle *ih, XKeyEvent *evt, Boolean *cont)
{
  KeySym motcode = XKeycodeToKeysym(iupmot_display, evt->keycode, 0);
  if (motcode == XK_Return)
    motTreeCallRenameCb(ih);
  else if (motcode == XK_Escape)
  {
    Widget wEdit = (Widget)iupAttribGet(ih, "_IUPTREE_EDITFIELD");
    Widget wItem = (Widget)iupAttribGet(ih, "_IUPTREE_SELECTED");

    XtDestroyWidget(wEdit);
    XmProcessTraversal(wItem, XmTRAVERSE_CURRENT);

    iupAttribSetStr(ih, "_IUPTREE_EDITFIELD", NULL);
    iupAttribSetStr(ih, "_IUPTREE_SELECTED",  NULL);
  }

  (void)cont;
  (void)w;
}

static void motTreeShowEditField(Ihandle* ih, Widget wItem)
{
  int num_args = 0;
  Arg args[30];
  Position xIcon, yIcon;
  Dimension wIcon, hIcon;
  char* child_id = iupDialogGetChildIdStr(ih);
  Widget cbEdit;
  XmString title;
  char* value;
  Pixel color;
  XmFontList fontlist;

  XtVaGetValues(wItem, XmNx, &xIcon, 
                       XmNy, &yIcon, 
                       XmNwidth, &wIcon, 
                       XmNheight, &hIcon, 
                       XmNlabelString, &title,
                       XmNforeground, &color,
                       XmNrenderTable, &fontlist,
                       NULL);

  iupmotSetArg(args, num_args, XmNx, xIcon+21);      /* x-position */
  iupmotSetArg(args, num_args, XmNy, yIcon);         /* y-position */
  iupmotSetArg(args, num_args, XmNwidth, wIcon-21);  /* default width to avoid 0 */
  iupmotSetArg(args, num_args, XmNheight, hIcon);    /* default height to avoid 0 */
  iupmotSetArg(args, num_args, XmNmarginHeight, 0);  /* default padding */
  iupmotSetArg(args, num_args, XmNmarginWidth, 0);
  iupmotSetArg(args, num_args, XmNforeground, color);
  iupmotSetArg(args, num_args, XmNrenderTable, fontlist);
  iupmotSetArg(args, num_args, XmNvalue, iupmotConvertString(title));
  iupmotSetArg(args, num_args, XmNtraversalOn, True);

  cbEdit = XtCreateManagedWidget(
    child_id,       /* child identifier */
    xmTextWidgetClass,   /* widget class */
    (Widget)iupAttribGet(ih, "_IUP_EXTRAPARENT"), /* widget parent */
    args, num_args);

  /* Disable Drag Source */
  iupmotDisableDragSource(cbEdit);

  XtAddEventHandler(cbEdit, EnterWindowMask, False, (XtEventHandler)iupmotEnterLeaveWindowEvent, (XtPointer)ih);
  XtAddEventHandler(cbEdit, LeaveWindowMask, False, (XtEventHandler)iupmotEnterLeaveWindowEvent, (XtPointer)ih);
  XtAddEventHandler(cbEdit, FocusChangeMask, False, (XtEventHandler)motTreeEditFocusChangeEvent, (XtPointer)ih);
  XtAddEventHandler(cbEdit, KeyPressMask,    False, (XtEventHandler)motTreeEditKeyPressEvent, (XtPointer)ih);

  iupAttribSetStr(ih, "_IUPTREE_SELECTED",  (char*)wItem);
  iupAttribSetStr(ih, "_IUPTREE_EDITFIELD", (char*)cbEdit);

  XmProcessTraversal(cbEdit, XmTRAVERSE_CURRENT);

  XmTextSetSelection(cbEdit, (XmTextPosition)0, (XmTextPosition)XmTextGetLastPosition(cbEdit), CurrentTime);

  value = iupAttribGetStr(ih, "RENAMECARET");
  if (value)
    motTreeSetRenameCaretPos(cbEdit, value);

  value = iupAttribGetStr(ih, "RENAMESELECTION");
  if (value)
    motTreeSetRenameSelectionPos(cbEdit, value);

  /* the parents callbacks can be called while editing 
     so we must avoid their processing if _IUPTREE_EDITFIELD is defined. */
}

static void motTreeSelectionCallback(Widget w, Ihandle* ih, XmContainerSelectCallbackStruct *nptr)
{
  IFnii cbSelec;
  int is_ctrl = 0;
  (void)w;
  (void)nptr;

  if (ih->data->mark_mode == ITREE_MARK_MULTIPLE)
  {
    char key[5];
    iupdrvGetKeyState(key);
    if (key[0] == 'S')
      return;
    else if (key[1] == 'C')
      is_ctrl = 1;
  }

  cbSelec = (IFnii)IupGetCallback(ih, "SELECTION_CB");
  if (cbSelec)
  {
    Widget wItemFocus = motTreeGetFocusNode(ih);
    int curpos = motTreeGetNodeId(ih, wItemFocus);
    if (is_ctrl) 
    {
      unsigned char isSelected;
      XtVaGetValues(wItemFocus, XmNvisualEmphasis, &isSelected, NULL);
      cbSelec(ih, curpos, isSelected == XmSELECTED? 1: 0);
    }
    else
    {
      int oldpos = iupAttribGetInt(ih, "_IUPTREE_OLDVALUE");
      if (oldpos != curpos)
      {
        cbSelec(ih, oldpos, 0);  /* unselected */
        cbSelec(ih, curpos, 1);  /*   selected */

        iupAttribSetInt(ih, "_IUPTREE_OLDVALUE", curpos);
      }
    }
  }
}

static void motTreeDefaultActionCallback(Widget w, Ihandle* ih, XmContainerSelectCallbackStruct *nptr)
{
  unsigned char itemState;
  WidgetList wSelectedItemList = NULL;
  int countItems;
  motTreeItemData *itemData;
  Widget wItem;
  (void)w;

  wSelectedItemList = nptr->selected_items;
  countItems = nptr->selected_item_count;

  if (!countItems || (Widget)iupAttribGet(ih, "_IUPTREE_EDITFIELD"))
    return;

  /* this works also when using multiple selection */
  wItem = wSelectedItemList[0];

  XtVaGetValues(wItem, XmNoutlineState, &itemState,
                       XmNuserData, &itemData, NULL);

  if (itemData->kind == ITREE_BRANCH)
  {
    if (itemState == XmEXPANDED)
      XtVaSetValues(wItem, XmNoutlineState,  XmCOLLAPSED, NULL);
    else
      XtVaSetValues(wItem, XmNoutlineState,  XmEXPANDED, NULL);
  }
  else
  {
    IFni cbExecuteLeaf = (IFni)IupGetCallback(ih, "EXECUTELEAF_CB");
    if (cbExecuteLeaf)
      cbExecuteLeaf(ih, motTreeGetNodeId(ih, wItem));
  }
}

static void motTreeOutlineChangedCallback(Widget w, Ihandle* ih, XmContainerOutlineCallbackStruct *nptr)
{
  motTreeItemData *itemData;
  XtVaGetValues(nptr->item, XmNuserData, &itemData, NULL);

  if (nptr->reason == XmCR_EXPANDED)
  {
    if (motTreeCallBranchOpenCb(ih, nptr->item) == IUP_IGNORE)
      nptr->new_outline_state = XmCOLLAPSED; /* prevent the change */
    else
    {
      XtVaSetValues(nptr->item, XmNsmallIconPixmap, (itemData->image_expanded!=XmUNSPECIFIED_PIXMAP)? itemData->image_expanded: (Pixmap)ih->data->def_image_expanded, NULL);
      XtVaSetValues(nptr->item, XmNsmallIconMask, (itemData->image_expanded_mask!=XmUNSPECIFIED_PIXMAP)? itemData->image_expanded_mask: (Pixmap)ih->data->def_image_expanded_mask, NULL);
    }
  }
  else if (nptr->reason == XmCR_COLLAPSED)
  {
    if (motTreeCallBranchCloseCb(ih, nptr->item) == IUP_IGNORE)
      nptr->new_outline_state = XmEXPANDED;  /* prevent the change */
    else
    {
      XtVaSetValues(nptr->item, XmNsmallIconPixmap, (itemData->image!=XmUNSPECIFIED_PIXMAP)? itemData->image: (Pixmap)ih->data->def_image_collapsed, NULL);
      XtVaSetValues(nptr->item, XmNsmallIconMask, (itemData->image_mask!=XmUNSPECIFIED_PIXMAP)? itemData->image_mask: (Pixmap)ih->data->def_image_collapsed_mask, NULL);
    }
  }

  (void)w;
}

static void motTreeTraverseObscuredCallback(Widget widget, Ihandle* ih, XmTraverseObscuredCallbackStruct *cbs)
{
  (void)ih;
  /* allow to do automatic scroll when navigating in the tree */
  XmScrollVisible(widget, cbs->traversal_destination, 10, 10);
}

static void motTreeKeyReleaseEvent(Widget w, Ihandle *ih, XKeyEvent *evt, Boolean *cont)
{
  KeySym motcode;

  if (iupAttribGet(ih, "_IUPTREE_EDITFIELD"))
    return;

  motcode = XKeycodeToKeysym(iupmot_display, evt->keycode, 0);
  if (motcode == XK_Down || motcode == XK_U || motcode == XK_Home || motcode == XK_End)
  {
    if (ih->data->mark_mode==ITREE_MARK_MULTIPLE && (evt->state & ShiftMask))
      motTreeCallMultiSelectionCb(ih);
  }

  (void)w;
  (void)cont;
}

static void motTreeKeyPressEvent(Widget w, Ihandle *ih, XKeyEvent *evt, Boolean *cont)
{
  KeySym motcode;

  if (iupAttribGet(ih, "_IUPTREE_EDITFIELD"))
    return;

  motcode = XKeycodeToKeysym(iupmot_display, evt->keycode, 0);
  if (motcode == XK_Tab || motcode == XK_KP_Tab)
  {
    if (evt->state & ShiftMask)
      IupPreviousField(ih);
    else
      IupNextField(ih);
    *cont = False;
    return;
  }
  else if (motcode == XK_F2)
  {
    motTreeSetRenameAttrib(ih, NULL);
    return;
  }
  else if ((motcode == XK_Down || motcode == XK_Up) && (evt->state & ControlMask))
  {
    Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
    Widget wItem;
    Widget wItemFocus = motTreeGetFocusNode(ih);

    /* Ctrl+Arrows move only focus */
    if (motcode == XK_Down)
      wItem = motTreeGetNextVisibleNode(ih, wRoot, wItemFocus);
    else
      wItem = motTreeGetPreviousVisibleNode(ih, wRoot, wItemFocus);

    XmProcessTraversal(wItem, XmTRAVERSE_CURRENT);
    *cont = False;
    return;
  }
  else if(motcode == XK_Home || motcode == XK_End)
  {
    Widget wRoot = (Widget)iupAttribGet(ih, "_IUPTREE_ROOTITEM");
    Widget wItem;

    /* Not processed by Motif */

    if (motcode == XK_Home)
      wItem = wRoot;
    else
      wItem = motTreeGetLastVisibleNode(ih, wRoot);

    /* Ctrl+Arrows move only focus */
    if (!(evt->state & ControlMask))
    {
      /* Shift+Arrows select block */
      if (evt->state & ShiftMask)
      {
        Widget wItemFocus = motTreeGetFocusNode(ih);
        if (!wItemFocus)
          return;
        motTreeSelectRange(ih, wItemFocus, wItem, 1);
      }
      else
      {
        /* clear previous selection */
        XtVaSetValues(ih->handle, XmNselectedObjects,  NULL, NULL);
        XtVaSetValues(ih->handle, XmNselectedObjectCount, 0, NULL);

        XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
      }
    }

    XmProcessTraversal(wItem, XmTRAVERSE_CURRENT);
    *cont = False;
    return;
  }
  else if(motcode == XK_space && (evt->state & ControlMask))
  {
    Widget wItemFocus = motTreeGetFocusNode(ih);
    if (wItemFocus)
    {
      unsigned char isSelected;
      XtVaGetValues(wItemFocus, XmNvisualEmphasis, &isSelected, NULL);
      if (isSelected == XmSELECTED)
        XtVaSetValues(wItemFocus, XmNvisualEmphasis, XmNOT_SELECTED, NULL);
      else
        XtVaSetValues(wItemFocus, XmNvisualEmphasis, XmSELECTED, NULL);
    }
  }

  iupmotKeyPressEvent(w, ih, (XEvent*)evt, cont);
}

static void motTreeButtonEvent(Widget w, Ihandle* ih, XButtonEvent* evt, Boolean* cont)
{
  (void)w;
  (void)cont;

  if (iupAttribGet(ih, "_IUPTREE_EDITFIELD"))
    return;

  if (evt->type==ButtonPress)
  {   
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_ENTERLEAVE", "1");

    if (evt->button==Button1)
    {
      Widget wItemFocus = motTreeGetFocusNode(ih);
      static Widget wLastItem = NULL;
      static Time last = 0;
      int clicktwice = 0, doubleclicktime = XtGetMultiClickTime(iupmot_display);
      int elapsed = (int)(evt->time - last);
      last = evt->time;

      /* stay away from double click and leave some room for clicks */
      if (elapsed > (3*doubleclicktime)/2 && elapsed <= 3*doubleclicktime)
        clicktwice = 1;
    
      if (clicktwice && wLastItem && wLastItem==wItemFocus)
      {
        motTreeSetRenameAttrib(ih, NULL);
        *cont = False;
      }
      wLastItem = wItemFocus;
    }
    else if (evt->button==Button3)
      motTreeCallRightClickCb(ih, evt->x, evt->y);
  }
  else if (evt->type==ButtonRelease)
  {   
    if (evt->button==Button1)
    {
      if (ih->data->mark_mode==ITREE_MARK_MULTIPLE && (evt->state & ShiftMask))
        motTreeCallMultiSelectionCb(ih);
    }
  }

  iupmotButtonPressReleaseEvent(w, ih, (XEvent*)evt, cont);
}

static void motTreeTransferProc(Widget drop_context, XtPointer client_data, Atom *seltype, Atom *type, XtPointer value, unsigned long *length, int format)
{
  Atom atomTreeItem = XInternAtom(iupmot_display, "TREE_ITEM", False);
  Widget wItemDrop = (Widget)client_data;
  Widget wItemDrag = (Widget)value;

  if (*type == atomTreeItem)
  {
    Widget wParent;
    Ihandle* ih = NULL;

    if (!wItemDrop || wItemDrag == wItemDrop)
      return;

    wParent = wItemDrop;
    while(wParent)
    {
      XtVaGetValues(wParent, XmNentryParent, &wParent, NULL);
      if (wParent == wItemDrag)
        return;
    }

    XtVaGetValues(XtParent(wItemDrag), XmNuserData, &ih, NULL);

    if (motTreeCallDragDropCb(ih, wItemDrag, wItemDrop) == IUP_CONTINUE)
    {
      /* Copy the dragged item to the new position. After, remove it */
      Widget wNewItem = motTreeCopyNode(ih, wItemDrag, wItemDrop);

      motTreeRemoveNode(ih, wItemDrag, 0);  /* do not delete the user data, we copy the references in CopyBranch */

      /* Select the dragged item */
      XtVaSetValues(ih->handle, XmNselectedObjects,  NULL, NULL);
      XtVaSetValues(ih->handle, XmNselectedObjectCount, 0, NULL);
      XtVaSetValues(wNewItem, XmNvisualEmphasis, XmSELECTED, NULL);

      XmProcessTraversal(wNewItem, XmTRAVERSE_CURRENT);
    }
  }

  (void)drop_context;
  (void)seltype;
  (void)format;
  (void)length;
}

static void motTreeDropProc(Widget w, XtPointer client_data, XmDropProcCallbackStruct* drop_data)
{
  Atom atomTreeItem = XInternAtom(iupmot_display, "TREE_ITEM", False);
  XmDropTransferEntryRec transferList[2];
  Arg args[10];
  int i, num_args = 0;
  Widget wItemDrop, drop_context;
  Cardinal numExportTargets;
  Atom *exportTargets;
  Boolean found = False;
  (void)client_data;

  drop_context = drop_data->dragContext;

  /* retrieve the data targets */
  iupmotSetArg(args, num_args, XmNexportTargets, &exportTargets);
  iupmotSetArg(args, num_args, XmNnumExportTargets, &numExportTargets);
  XtGetValues(drop_context, args, num_args);

  for (i = 0; i < (int)numExportTargets; i++) 
  {
    if (exportTargets[i] == atomTreeItem) 
    {
      found = True;
      break;
    }
  }

  wItemDrop = XmObjectAtPoint(w, drop_data->x, drop_data->y);
  if (!wItemDrop)
    found = False;

  num_args = 0;
  if ((!found) || (drop_data->dropAction != XmDROP) ||  (drop_data->operation != XmDROP_COPY && drop_data->operation != XmDROP_MOVE)) 
  {
    iupmotSetArg(args, num_args, XmNtransferStatus, XmTRANSFER_FAILURE);
    iupmotSetArg(args, num_args, XmNnumDropTransfers, 0);
  }
  else 
  {
    /* set up transfer requests for drop site */
    transferList[0].target = atomTreeItem;
    transferList[0].client_data = (XtPointer)wItemDrop;
    iupmotSetArg(args, num_args, XmNdropTransfers, transferList);
    iupmotSetArg(args, num_args, XmNnumDropTransfers, 1);
    iupmotSetArg(args, num_args, XmNtransferProc, motTreeTransferProc);
  }

  XmDropTransferStart(drop_context, args, num_args);
}

static void motTreeDragDropFinishCallback(Widget drop_context, XtPointer client_data, XtPointer call_data)
{
  Widget source_icon = NULL;
  XtVaGetValues(drop_context, XmNsourceCursorIcon, &source_icon, NULL);
  if (source_icon)
    XtDestroyWidget(source_icon);
  (void)call_data;
  (void)client_data;
}

static void motTreeDragMotionCallback(Widget drop_context, Widget wItemDrag, XmDragMotionCallbackStruct* drag_motion)
{
  Ihandle* ih = NULL;
  XtVaGetValues(XtParent(wItemDrag), XmNuserData, &ih, NULL);
  if (!iupAttribGet(ih, "NODRAGFEEDBACK"))
  {
    Widget wItem;
    int x = drag_motion->x;
    int y = drag_motion->y;
    iupdrvScreenToClient(ih, &x, &y);
    wItem = XmObjectAtPoint(ih->handle, (Position)x, (Position)y);
    if (wItem)
    {
      XtVaSetValues(ih->handle, XmNselectedObjects,  NULL, NULL);
      XtVaSetValues(ih->handle, XmNselectedObjectCount, 0, NULL);
      XtVaSetValues(wItem, XmNvisualEmphasis, XmSELECTED, NULL);
    }
  }
  (void)drop_context;
}

static Boolean motTreeConvertProc(Widget drop_context, Atom *selection, Atom *target, Atom *type_return,
                                  XtPointer *value_return, unsigned long *length_return, int *format_return)
{
  Atom atomMotifDrop = XInternAtom(iupmot_display, "_MOTIF_DROP", False);
  Atom atomTreeItem = XInternAtom(iupmot_display, "TREE_ITEM", False);
  Widget wItemDrag = NULL;

  /* check if we are dealing with a drop */
  if (*selection != atomMotifDrop || *target != atomTreeItem)
    return False;

  XtVaGetValues(drop_context, XmNclientData, &wItemDrag, NULL);
  if (!wItemDrag)
    return False;

  /* format the value for transfer */
  *type_return = atomTreeItem;
  *value_return = (XtPointer)wItemDrag;
  *length_return = 1;
  *format_return = 32;
  return True;
}

static void motTreeStartDrag(Widget w, XButtonEvent* evt, String* params, Cardinal* num_params)
{
  Atom atomTreeItem = XInternAtom(iupmot_display, "TREE_ITEM", False);
  Atom exportList[1];
  Widget drag_icon, drop_context;
  Pixmap pixmap = 0, mask = 0;
  int num_args = 0;
  Arg args[40];
  Pixel fg, bg;
  Widget wItemDrag = XmObjectAtPoint(w, (Position)evt->x, (Position)evt->y);
  if (!wItemDrag)
    return;

  XtVaGetValues(wItemDrag, XmNsmallIconPixmap, &pixmap, 
                           XmNsmallIconMask, &mask, 
                           XmNbackground, &bg,
                           XmNforeground, &fg,
                           NULL);

  iupmotSetArg(args, num_args, XmNpixmap, pixmap);
  iupmotSetArg(args, num_args, XmNmask, mask);
  drag_icon = XmCreateDragIcon(w, "drag_icon", args, num_args);

  exportList[0] = atomTreeItem;

  /* specify resources for DragContext for the transfer */
  num_args = 0;
  iupmotSetArg(args, num_args, XmNcursorBackground, bg);
  iupmotSetArg(args, num_args, XmNcursorForeground, fg);
  /* iupmotSetArg(args, num_args, XmNsourcePixmapIcon, drag_icon);  works, but only outside the dialog, inside disapears */
  iupmotSetArg(args, num_args, XmNsourceCursorIcon, drag_icon);  /* does not work, shows the default cursor */
  iupmotSetArg(args, num_args, XmNexportTargets, exportList);
  iupmotSetArg(args, num_args, XmNnumExportTargets, 1);
  iupmotSetArg(args, num_args, XmNdragOperations, XmDROP_MOVE|XmDROP_COPY);
  iupmotSetArg(args, num_args, XmNconvertProc, motTreeConvertProc);
  iupmotSetArg(args, num_args, XmNclientData, wItemDrag);

  /* start the drag and register a callback to clean up when done */
  drop_context = XmDragStart(w, (XEvent*)evt, args, num_args);
  XtAddCallback(drop_context, XmNdragDropFinishCallback, (XtCallbackProc)motTreeDragDropFinishCallback, NULL);
  XtAddCallback(drop_context, XmNdragMotionCallback, (XtCallbackProc)motTreeDragMotionCallback, (XtPointer)wItemDrag);

  (void)params;
  (void)num_params;
}

static void motTreeEnableDragDrop(Widget w)
{
  Atom atomTreeItem = XInternAtom(iupmot_display, "TREE_ITEM", False);
  Atom importList[1];
  Arg args[40];
  int num_args = 0;
  char dragTranslations[] = "#override <Btn2Down>: StartDrag()";
  static int do_rec = 0;
  if (!do_rec)
  {
    XtActionsRec rec = {"StartDrag", (XtActionProc)motTreeStartDrag};
    XtAppAddActions(iupmot_appcontext, &rec, 1);
    do_rec = 1;
  }
  XtOverrideTranslations(w, XtParseTranslationTable(dragTranslations));

  importList[0] = atomTreeItem;
  iupmotSetArg(args, num_args, XmNimportTargets, importList);
  iupmotSetArg(args, num_args, XmNnumImportTargets, 1);
  iupmotSetArg(args, num_args, XmNdropSiteOperations, XmDROP_MOVE|XmDROP_COPY);
  iupmotSetArg(args, num_args, XmNdropProc, motTreeDropProc);
  XmDropSiteUpdate(w, args, num_args);

  XtVaSetValues(XmGetXmDisplay(iupmot_display), XmNenableDragIcon, True, NULL);
}

static int motTreeMapMethod(Ihandle* ih)
{
  int num_args = 0;
  Arg args[40];
  Widget parent = iupChildTreeGetNativeParentHandle(ih);
  char* child_id = iupDialogGetChildIdStr(ih);
  Widget sb_win;

  /******************************/
  /* Create the scrolled window */
  /******************************/
  iupmotSetArg(args, num_args, XmNmappedWhenManaged, False);  /* not visible when managed */
  iupmotSetArg(args, num_args, XmNscrollingPolicy, XmAUTOMATIC);
  iupmotSetArg(args, num_args, XmNvisualPolicy, XmVARIABLE); 
  iupmotSetArg(args, num_args, XmNscrollBarDisplayPolicy, XmAS_NEEDED);
  iupmotSetArg(args, num_args, XmNspacing, 0); /* no space between scrollbars and text */
  iupmotSetArg(args, num_args, XmNborderWidth, 0);
  iupmotSetArg(args, num_args, XmNshadowThickness, 2);

  sb_win = XtCreateManagedWidget(
    child_id,  /* child identifier */
    xmScrolledWindowWidgetClass, /* widget class */
    parent,                      /* widget parent */
    args, num_args);

  if (!sb_win)
     return IUP_ERROR;

  XtAddCallback (sb_win, XmNtraverseObscuredCallback, (XtCallbackProc)motTreeTraverseObscuredCallback, (XtPointer)ih);

  parent = sb_win;
  child_id = "container";

  num_args = 0;
 
  iupmotSetArg(args, num_args, XmNx, 0);  /* x-position */
  iupmotSetArg(args, num_args, XmNy, 0);  /* y-position */
  iupmotSetArg(args, num_args, XmNwidth, 10);  /* default width to avoid 0 */
  iupmotSetArg(args, num_args, XmNheight, 10); /* default height to avoid 0 */

  iupmotSetArg(args, num_args, XmNmarginHeight, 0);  /* default padding */
  iupmotSetArg(args, num_args, XmNmarginWidth, 0);

  if (iupStrBoolean(iupAttribGetStr(ih, "CANFOCUS")))
    iupmotSetArg(args, num_args, XmNtraversalOn, True);
  else
    iupmotSetArg(args, num_args, XmNtraversalOn, False);

  iupmotSetArg(args, num_args, XmNnavigationType, XmTAB_GROUP);
  iupmotSetArg(args, num_args, XmNhighlightThickness, 2);
  iupmotSetArg(args, num_args, XmNshadowThickness, 0);

  iupmotSetArg(args, num_args, XmNlayoutType, XmOUTLINE);
  iupmotSetArg(args, num_args, XmNentryViewType, XmSMALL_ICON);
  iupmotSetArg(args, num_args, XmNselectionPolicy, XmSINGLE_SELECT);
  iupmotSetArg(args, num_args, XmNoutlineIndentation, 20);

  if (iupAttribGetInt(ih, "HIDELINES"))
    iupmotSetArg(args, num_args, XmNoutlineLineStyle,  XmNO_LINE);
  else
    iupmotSetArg(args, num_args, XmNoutlineLineStyle, XmSINGLE);

  if (iupAttribGetInt(ih, "HIDEBUTTONS"))
    iupmotSetArg(args, num_args, XmNoutlineButtonPolicy,  XmOUTLINE_BUTTON_ABSENT);
  else
    iupmotSetArg(args, num_args, XmNoutlineButtonPolicy, XmOUTLINE_BUTTON_PRESENT);

  ih->handle = XtCreateManagedWidget(
    child_id,       /* child identifier */
    xmContainerWidgetClass,   /* widget class */
    parent,         /* widget parent */
    args, num_args);

  if (!ih->handle)
    return IUP_ERROR;

  ih->serial = iupDialogGetChildId(ih); /* must be after using the string */

  iupAttribSetStr(ih, "_IUP_EXTRAPARENT", (char*)parent);

  XtAddEventHandler(ih->handle, EnterWindowMask, False, (XtEventHandler)motTreeEnterLeaveWindowEvent, (XtPointer)ih);
  XtAddEventHandler(ih->handle, LeaveWindowMask, False, (XtEventHandler)motTreeEnterLeaveWindowEvent, (XtPointer)ih);
  XtAddEventHandler(ih->handle, FocusChangeMask, False, (XtEventHandler)motTreeFocusChangeEvent,  (XtPointer)ih);
  XtAddEventHandler(ih->handle, KeyPressMask,    False, (XtEventHandler)motTreeKeyPressEvent,    (XtPointer)ih);
  XtAddEventHandler(ih->handle, KeyReleaseMask,  False, (XtEventHandler)motTreeKeyReleaseEvent,  (XtPointer)ih);
  XtAddEventHandler(ih->handle, ButtonPressMask|ButtonReleaseMask, False, (XtEventHandler)motTreeButtonEvent, (XtPointer)ih);
  XtAddEventHandler(ih->handle, PointerMotionMask, False, (XtEventHandler)iupmotPointerMotionEvent, (XtPointer)ih);

  /* Callbacks */
  XtAddCallback(ih->handle, XmNhelpCallback, (XtCallbackProc)iupmotHelpCallback, (XtPointer)ih);
  XtAddCallback(ih->handle, XmNoutlineChangedCallback, (XtCallbackProc)motTreeOutlineChangedCallback, (XtPointer)ih);
  XtAddCallback(ih->handle, XmNdefaultActionCallback,  (XtCallbackProc)motTreeDefaultActionCallback,  (XtPointer)ih);
  XtAddCallback(ih->handle, XmNselectionCallback,      (XtCallbackProc)motTreeSelectionCallback,      (XtPointer)ih);

  XtRealizeWidget(parent);

  if (ih->data->show_dragdrop)
  {
    motTreeEnableDragDrop(ih->handle);
    XtVaSetValues(ih->handle, XmNuserData, ih, NULL);  /* to be used in motTreeTransferProc */
  }
  else
    iupmotDisableDragSource(ih->handle);

  /* Force background update before setting the images */
  {
    char* value = iupAttribGet(ih, "BGCOLOR");
    if (value)
    {
      motTreeSetBgColorAttrib(ih, value);
      iupAttribSetStr(ih, "BGCOLOR", NULL);
    }
  }

  /* Initialize the default images */
  ih->data->def_image_leaf = iupImageGetImage("IMGLEAF", ih, 0, "IMAGELEAF");
  if (!ih->data->def_image_leaf) 
  {
    ih->data->def_image_leaf = (void*)XmUNSPECIFIED_PIXMAP;
    ih->data->def_image_leaf_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    ih->data->def_image_leaf_mask = iupImageGetMask("IMGLEAF");
    if (!ih->data->def_image_leaf_mask) ih->data->def_image_leaf_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }

  ih->data->def_image_collapsed = iupImageGetImage("IMGCOLLAPSED", ih, 0, "IMAGEBRANCHCOLLAPSED");
  if (!ih->data->def_image_collapsed) 
  {
    ih->data->def_image_collapsed = (void*)XmUNSPECIFIED_PIXMAP;
    ih->data->def_image_collapsed_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    ih->data->def_image_collapsed_mask = iupImageGetMask("IMGCOLLAPSED");
    if (!ih->data->def_image_collapsed_mask) ih->data->def_image_collapsed_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }

  ih->data->def_image_expanded = iupImageGetImage("IMGEXPANDED", ih, 0, "IMAGEBRANCHEXPANDED");
  if (!ih->data->def_image_expanded) 
  {
    ih->data->def_image_expanded = (void*)XmUNSPECIFIED_PIXMAP;
    ih->data->def_image_expanded_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }
  else
  {
    ih->data->def_image_expanded_mask = iupImageGetMask("IMGEXPANDED");
    if (!ih->data->def_image_expanded_mask) ih->data->def_image_expanded_mask = (void*)XmUNSPECIFIED_PIXMAP;
  }

  motTreeAddRootNode(ih);

  IupSetCallback(ih, "_IUP_XY2POS_CB", (Icallback)motTreeConvertXYToPos);

  return IUP_NOERROR;
}

void iupdrvTreeInitClass(Iclass* ic)
{
  /* Driver Dependent Class functions */
  ic->Map = motTreeMapMethod;

  /* Visual */
  iupClassRegisterAttribute(ic, "BGCOLOR", NULL, motTreeSetBgColorAttrib, "TXTBGCOLOR", NULL, IUPAF_DEFAULT);
  iupClassRegisterAttribute(ic, "FGCOLOR", NULL, motTreeSetFgColorAttrib, IUPAF_SAMEASSYSTEM, "TXTFGCOLOR", IUPAF_DEFAULT);

  /* IupTree Attributes - GENERAL */
  iupClassRegisterAttribute(ic, "EXPANDALL",  NULL, motTreeSetExpandAllAttrib,  NULL, NULL, IUPAF_WRITEONLY||IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "INDENTATION",  motTreeGetIndentationAttrib, motTreeSetIndentationAttrib, NULL, NULL, IUPAF_DEFAULT);
  iupClassRegisterAttribute(ic, "COUNT", motTreeGetCountAttrib, NULL, NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_READONLY|IUPAF_NO_INHERIT);

  /* IupTree Attributes - IMAGES */
  iupClassRegisterAttributeId(ic, "IMAGE", NULL, motTreeSetImageAttrib, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "IMAGEEXPANDED", NULL, motTreeSetImageExpandedAttrib, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "IMAGELEAF",            NULL, motTreeSetImageLeafAttrib, IUPAF_SAMEASSYSTEM, "IMGLEAF", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGEBRANCHCOLLAPSED", NULL, motTreeSetImageBranchCollapsedAttrib, IUPAF_SAMEASSYSTEM, "IMGCOLLAPSED", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGEBRANCHEXPANDED",  NULL, motTreeSetImageBranchExpandedAttrib, IUPAF_SAMEASSYSTEM, "IMGEXPANDED", IUPAF_NO_INHERIT);

  /* IupTree Attributes - NODES */
  iupClassRegisterAttributeId(ic, "STATE",  motTreeGetStateAttrib,  motTreeSetStateAttrib, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "DEPTH",  motTreeGetDepthAttrib,  NULL, IUPAF_READONLY|IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "KIND",   motTreeGetKindAttrib,   NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "PARENT", motTreeGetParentAttrib, NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "COLOR",  motTreeGetColorAttrib,  motTreeSetColorAttrib, IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "NAME",   motTreeGetTitleAttrib,   motTreeSetTitleAttrib, IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "TITLE",   motTreeGetTitleAttrib,   motTreeSetTitleAttrib, IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "USERDATA",   motTreeGetUserDataAttrib,   motTreeSetUserDataAttrib, IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "CHILDCOUNT",   motTreeGetChildCountAttrib,   NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "TITLEFONT", motTreeGetTitleFontAttrib, motTreeSetTitleFontAttrib, IUPAF_NO_INHERIT);

  /* IupTree Attributes - MARKS */
  iupClassRegisterAttributeId(ic, "MARKED",   motTreeGetMarkedAttrib,   motTreeSetMarkedAttrib,   IUPAF_NO_INHERIT);
  iupClassRegisterAttribute  (ic, "MARK",    NULL,    motTreeSetMarkAttrib,    NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute  (ic, "STARTING", NULL, motTreeSetMarkStartAttrib, NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute  (ic, "MARKSTART", NULL, motTreeSetMarkStartAttrib, NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);

  iupClassRegisterAttribute  (ic, "VALUE",    motTreeGetValueAttrib,    motTreeSetValueAttrib,    NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);

  /* IupTree Attributes - ACTION */
  iupClassRegisterAttributeId(ic, "DELNODE", NULL, motTreeSetDelNodeAttrib, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "RENAME",  NULL, motTreeSetRenameAttrib,  NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "MOVENODE",  NULL, motTreeSetMoveNodeAttrib,  IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
}
