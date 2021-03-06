/** \file
 * \brief User API
 * IUP - A Portable User Interface Toolkit
 * Tecgraf: Computer Graphics Technology Group, PUC-Rio, Brazil
 * http://www.tecgraf.puc-rio.br/iup  mailto:iup@tecgraf.puc-rio.br
 *
 * See Copyright Notice at the end of this file
 */
 
#ifndef __IUP_H 
#define __IUP_H

#include <iupkey.h>
#include <iupdef.h>

#ifdef __cplusplus
extern "C" {
#endif


#define IUP_NAME "IUP - Portable User Interface"
#define IUP_COPYRIGHT  "Copyright (C) 1994-2012 Tecgraf, PUC-Rio."
#define IUP_DESCRIPTION	"Multi-platform toolkit for building graphical user interfaces."
#define IUP_VERSION "3.6"         /* bug fixes are reported only by IupVersion functions */
#define IUP_VERSION_NUMBER 306000
#define IUP_VERSION_DATE "2012/06/23"  /* does not include bug fix releases */

typedef struct Ihandle_ Ihandle;
typedef int (*Icallback)(Ihandle*);

/************************************************************************/
/*                        Main API                                      */
/************************************************************************/

int       IupOpen          (int *argc, char ***argv);
void      IupClose         (void);
void      IupImageLibOpen  (void);

int       IupMainLoop      (void);
int       IupLoopStep      (void);
int       IupLoopStepWait  (void);
int       IupMainLoopLevel (void);
void      IupFlush         (void);
void      IupExitLoop      (void);

int       IupRecordInput(const char* filename, int mode);
int       IupPlayInput(const char* filename);

void      IupUpdate        (Ihandle* ih);
void      IupUpdateChildren(Ihandle* ih);
void      IupRedraw        (Ihandle* ih, int children);
void      IupRefresh       (Ihandle* ih);
void      IupRefreshChildren(Ihandle* ih);

char*     IupMapFont       (const char *iupfont);
char*     IupUnMapFont     (const char *driverfont);
int       IupHelp          (const char* url);
char*     IupLoad          (const char *filename);
char*     IupLoadBuffer    (const char *buffer);

char*     IupVersion       (void);
char*     IupVersionDate   (void);
int       IupVersionNumber (void);
void      IupSetLanguage   (const char *lng);
char*     IupGetLanguage   (void);

void      IupDestroy      (Ihandle* ih);
void      IupDetach       (Ihandle* child);
Ihandle*  IupAppend       (Ihandle* ih, Ihandle* child);
Ihandle*  IupInsert       (Ihandle* ih, Ihandle* ref_child, Ihandle* child);
Ihandle*  IupGetChild     (Ihandle* ih, int pos);
int       IupGetChildPos  (Ihandle* ih, Ihandle* child);
int       IupGetChildCount(Ihandle* ih);
Ihandle*  IupGetNextChild (Ihandle* ih, Ihandle* child);
Ihandle*  IupGetBrother   (Ihandle* ih);
Ihandle*  IupGetParent    (Ihandle* ih);
Ihandle*  IupGetDialog    (Ihandle* ih);
Ihandle*  IupGetDialogChild(Ihandle* ih, const char* name);
int       IupReparent     (Ihandle* ih, Ihandle* new_parent, Ihandle* ref_child);

int       IupPopup         (Ihandle* ih, int x, int y);
int       IupShow          (Ihandle* ih);
int       IupShowXY        (Ihandle* ih, int x, int y);
int       IupHide          (Ihandle* ih);
int       IupMap           (Ihandle* ih);
void      IupUnmap         (Ihandle *ih);

void      IupSetAttribute  (Ihandle* ih, const char* name, const char* value);
void      IupStoreAttribute(Ihandle* ih, const char* name, const char* value);
Ihandle*  IupSetAttributes (Ihandle* ih, const char *str);
char*     IupGetAttribute  (Ihandle* ih, const char* name);
char*     IupGetAttributes (Ihandle* ih);
int       IupGetInt        (Ihandle* ih, const char* name);
int       IupGetInt2       (Ihandle* ih, const char* name);
int       IupGetIntInt     (Ihandle *ih, const char* name, int *i1, int *i2);
float     IupGetFloat      (Ihandle* ih, const char* name);
void      IupSetfAttribute (Ihandle* ih, const char* name, const char* format, ...);
void      IupResetAttribute(Ihandle *ih, const char* name);
int       IupGetAllAttributes(Ihandle* ih, char** names, int n);
Ihandle*  IupSetAtt(const char* handle_name, Ihandle* ih, const char* name, ...);

void  IupSetAttributeId(Ihandle *ih, const char* name, int id, const char *value);
void  IupStoreAttributeId(Ihandle *ih, const char* name, int id, const char *value);
char* IupGetAttributeId(Ihandle *ih, const char* name, int id);
float IupGetFloatId(Ihandle *ih, const char* name, int id);
int   IupGetIntId(Ihandle *ih, const char* name, int id);
void  IupSetfAttributeId(Ihandle *ih, const char* name, int id, const char* format, ...);

void  IupSetAttributeId2(Ihandle* ih, const char* name, int lin, int col, const char* value);
void  IupStoreAttributeId2(Ihandle* ih, const char* name, int lin, int col, const char* value);
char* IupGetAttributeId2(Ihandle* ih, const char* name, int lin, int col);
int   IupGetIntId2(Ihandle* ih, const char* name, int lin, int col);
float IupGetFloatId2(Ihandle* ih, const char* name, int lin, int col);
void  IupSetfAttributeId2(Ihandle* ih, const char* name, int lin, int col, const char* format, ...);

void      IupSetGlobal     (const char* name, const char* value);
void      IupStoreGlobal   (const char* name, const char* value);
char*     IupGetGlobal     (const char* name);

Ihandle*  IupSetFocus      (Ihandle* ih);
Ihandle*  IupGetFocus      (void);
Ihandle*  IupPreviousField (Ihandle* ih);  
Ihandle*  IupNextField     (Ihandle* ih);

Icallback IupGetCallback(Ihandle* ih, const char *name);
Icallback IupSetCallback(Ihandle* ih, const char *name, Icallback func);
Ihandle*  IupSetCallbacks(Ihandle* ih, const char *name, Icallback func, ...);

Icallback   IupGetFunction   (const char *name);
Icallback   IupSetFunction   (const char *name, Icallback func);
const char* IupGetActionName (void);

Ihandle*  IupGetHandle     (const char *name);
Ihandle*  IupSetHandle     (const char *name, Ihandle* ih);
int       IupGetAllNames   (char** names, int n);
int       IupGetAllDialogs (char** names, int n);
char*     IupGetName       (Ihandle* ih);

void      IupSetAttributeHandle(Ihandle* ih, const char* name, Ihandle* ih_named);
Ihandle*  IupGetAttributeHandle(Ihandle* ih, const char* name);

char*     IupGetClassName(Ihandle* ih);
char*     IupGetClassType(Ihandle* ih);
int       IupGetAllClasses(char** names, int n);
int       IupGetClassAttributes(const char* classname, char** names, int n);
int       IupGetClassCallbacks(const char* classname, char** names, int n);
void      IupSaveClassAttributes(Ihandle* ih);
void      IupCopyClassAttributes(Ihandle* src_ih, Ihandle* dst_ih);
void      IupSetClassDefaultAttribute(const char* classname, const char *name, const char* value);
int       IupClassMatch(Ihandle* ih, const char* classname);

Ihandle*  IupCreate (const char *classname);
Ihandle*  IupCreatev(const char *classname, void* *params);
Ihandle*  IupCreatep(const char *classname, void *first, ...);

/************************************************************************/
/*                        Elements                                      */
/************************************************************************/

Ihandle*  IupFill       (void);
Ihandle*  IupRadio      (Ihandle* child);
Ihandle*  IupVbox       (Ihandle* child, ...);
Ihandle*  IupVboxv      (Ihandle* *children);
Ihandle*  IupZbox       (Ihandle* child, ...);
Ihandle*  IupZboxv      (Ihandle* *children);
Ihandle*  IupHbox       (Ihandle* child,...);
Ihandle*  IupHboxv      (Ihandle* *children);

Ihandle*  IupNormalizer (Ihandle* ih_first, ...);
Ihandle*  IupNormalizerv(Ihandle* *ih_list);

Ihandle*  IupCbox       (Ihandle* child, ...);
Ihandle*  IupCboxv      (Ihandle* *children);
Ihandle*  IupSbox       (Ihandle *child);
Ihandle*  IupSplit      (Ihandle* child1, Ihandle* child2);
Ihandle*  IupScrollBox  (Ihandle* child);

Ihandle*  IupFrame      (Ihandle* child);

Ihandle*  IupImage      (int width, int height, const unsigned char *pixmap);
Ihandle*  IupImageRGB   (int width, int height, const unsigned char *pixmap);
Ihandle*  IupImageRGBA  (int width, int height, const unsigned char *pixmap);

Ihandle*  IupItem       (const char* title, const char* action);
Ihandle*  IupSubmenu    (const char* title, Ihandle* child);
Ihandle*  IupSeparator  (void);
Ihandle*  IupMenu       (Ihandle* child,...);
Ihandle*  IupMenuv      (Ihandle* *children);

Ihandle*  IupButton     (const char* title, const char* action);
Ihandle*  IupCanvas     (const char* action);
Ihandle*  IupDialog     (Ihandle* child);
Ihandle*  IupUser       (void);
Ihandle*  IupLabel      (const char* title);
Ihandle*  IupList       (const char* action);
Ihandle*  IupText       (const char* action);
Ihandle*  IupMultiLine  (const char* action);
Ihandle*  IupToggle     (const char* title, const char* action);
Ihandle*  IupTimer      (void);
Ihandle*  IupClipboard  (void);
Ihandle*  IupProgressBar(void);
Ihandle*  IupVal        (const char *type);
Ihandle*  IupTabs       (Ihandle* child, ...);
Ihandle*  IupTabsv      (Ihandle* *children);
Ihandle*  IupTree       (void);

/* Deprecated controls use SPIN attribute of IupText */
Ihandle*  IupSpin       (void);
Ihandle*  IupSpinbox    (Ihandle* child);


/* IupImage utility */
int IupSaveImageAsText(Ihandle* ih, const char* file_name, const char* format, const char* name);

/* IupText utilities */
void  IupTextConvertLinColToPos(Ihandle* ih, int lin, int col, int *pos);
void  IupTextConvertPosToLinCol(Ihandle* ih, int pos, int *lin, int *col);

/* IupText, IupList and IupTree utility */
int   IupConvertXYToPos(Ihandle* ih, int x, int y);

/* IupTree utilities */
int   IupTreeSetUserId(Ihandle* ih, int id, void* userid);
void* IupTreeGetUserId(Ihandle* ih, int id);
int   IupTreeGetId(Ihandle* ih, void *userid);

/* Deprecated IupTree utilities, use Iup*AttributeId functions */
void  IupTreeSetAttribute  (Ihandle* ih, const char* name, int id, const char* value);
void  IupTreeStoreAttribute(Ihandle* ih, const char* name, int id, const char* value);
char* IupTreeGetAttribute  (Ihandle* ih, const char* name, int id);
int   IupTreeGetInt        (Ihandle* ih, const char* name, int id);
float IupTreeGetFloat      (Ihandle* ih, const char* name, int id);
void  IupTreeSetfAttribute (Ihandle* ih, const char* name, int id, const char* format, ...);
void  IupTreeSetAttributeHandle(Ihandle* ih, const char* a, int id, Ihandle* ih_named);


/************************************************************************/
/*                      Pre-definided dialogs                           */
/************************************************************************/

Ihandle* IupFileDlg(void);
Ihandle* IupMessageDlg(void);
Ihandle* IupColorDlg(void);
Ihandle* IupFontDlg(void);

int  IupGetFile(char *arq);
void IupMessage(const char *title, const char *msg);
void IupMessagef(const char *title, const char *format, ...);
int  IupAlarm(const char *title, const char *msg, const char *b1, const char *b2, const char *b3);
int  IupScanf(const char *format, ...);
int  IupListDialog(int type, const char *title, int size, const char** list,
                   int op, int max_col, int max_lin, int* marks);
int  IupGetText(const char* title, char* text);
int  IupGetColor(int x, int y, unsigned char* r, unsigned char* g, unsigned char* b);

typedef int (*Iparamcb)(Ihandle* dialog, int param_index, void* user_data);
int IupGetParam(const char* title, Iparamcb action, void* user_data, const char* format,...);
int IupGetParamv(const char* title, Iparamcb action, void* user_data, const char* format, int param_count, int param_extra, void** param_data);

Ihandle* IupLayoutDialog(Ihandle* dialog);
Ihandle* IupElementPropertiesDialog(Ihandle* elem);


#ifdef __cplusplus
}
#endif

/************************************************************************/
/*                   Common Return Values                               */
/************************************************************************/
#define IUP_ERROR     1
#define IUP_NOERROR   0
#define IUP_OPENED   -1
#define IUP_INVALID  -1

/************************************************************************/
/*                   Callback Return Values                             */
/************************************************************************/
#define IUP_IGNORE    -1
#define IUP_DEFAULT   -2
#define IUP_CLOSE     -3
#define IUP_CONTINUE  -4

/************************************************************************/
/*           IupPopup and IupShowXY Parameter Values                    */
/************************************************************************/
#define IUP_CENTER        0xFFFF  /* 65535 */
#define IUP_LEFT          0xFFFE  /* 65534 */
#define IUP_RIGHT         0xFFFD  /* 65533 */
#define IUP_MOUSEPOS      0xFFFC  /* 65532 */
#define IUP_CURRENT       0xFFFB  /* 65531 */
#define IUP_CENTERPARENT  0xFFFA  /* 65530 */
#define IUP_TOP       IUP_LEFT
#define IUP_BOTTOM    IUP_RIGHT

/************************************************************************/
/*               SHOW_CB Callback Values                                */
/************************************************************************/
enum{IUP_SHOW, IUP_RESTORE, IUP_MINIMIZE, IUP_MAXIMIZE, IUP_HIDE};

/************************************************************************/
/*               SCROLL_CB Callback Values                              */
/************************************************************************/
enum{IUP_SBUP,   IUP_SBDN,    IUP_SBPGUP,   IUP_SBPGDN,    IUP_SBPOSV, IUP_SBDRAGV, 
     IUP_SBLEFT, IUP_SBRIGHT, IUP_SBPGLEFT, IUP_SBPGRIGHT, IUP_SBPOSH, IUP_SBDRAGH};

/************************************************************************/
/*               Mouse Button Values and Macros                         */
/************************************************************************/
#define IUP_BUTTON1   '1'
#define IUP_BUTTON2   '2'
#define IUP_BUTTON3   '3'
#define IUP_BUTTON4   '4'
#define IUP_BUTTON5   '5'

#define iup_isshift(_s)    (_s[0]=='S')
#define iup_iscontrol(_s)  (_s[1]=='C')
#define iup_isbutton1(_s)  (_s[2]=='1')
#define iup_isbutton2(_s)  (_s[3]=='2')
#define iup_isbutton3(_s)  (_s[4]=='3')
#define iup_isdouble(_s)   (_s[5]=='D')
#define iup_isalt(_s)      (_s[6]=='A')
#define iup_issys(_s)      (_s[7]=='Y')
#define iup_isbutton4(_s)  (_s[8]=='4')
#define iup_isbutton5(_s)  (_s[9]=='5')

/* Old definitions for backward compatibility */
#define isshift     iup_isshift
#define iscontrol   iup_iscontrol
#define isbutton1   iup_isbutton1
#define isbutton2   iup_isbutton2
#define isbutton3   iup_isbutton3
#define isdouble    iup_isdouble
#define isalt       iup_isalt
#define issys       iup_issys
#define isbutton4   iup_isbutton4
#define isbutton5   iup_isbutton5


/************************************************************************/
/*                      Pre-Defined Masks                               */
/************************************************************************/
#define IUP_MASK_FLOAT    "[+/-]?(/d+/.?/d*|/./d+)"
#define IUP_MASK_UFLOAT   "(/d+/.?/d*|/./d+)"
#define IUP_MASK_EFLOAT   "[+/-]?(/d+/.?/d*|/./d+)([eE][+/-]?/d+)?"
#define IUP_MASK_INT      "[+/-]?/d+"
#define IUP_MASK_UINT     "/d+"

/* Old definitions for backward compatibility */
#define IUPMASK_FLOAT     IUP_MASK_FLOAT
#define IUPMASK_UFLOAT    IUP_MASK_UFLOAT
#define IUPMASK_EFLOAT    IUP_MASK_EFLOAT
#define IUPMASK_INT	      IUP_MASK_INT
#define IUPMASK_UINT      IUP_MASK_UINT


/************************************************************************/
/*                   IupGetParam Callback situations                    */
/************************************************************************/
#define IUP_GETPARAM_OK     -1
#define IUP_GETPARAM_INIT   -2
#define IUP_GETPARAM_CANCEL -3
#define IUP_GETPARAM_HELP   -4


/************************************************************************/
/*                   Record Input Modes                                 */
/************************************************************************/
enum {IUP_RECBINARY, IUP_RECTEXT};


/************************************************************************/
/*              Replacement for the WinMain in Windows,                 */
/*        this allows the application to start from "main".             */
/*        Used only for Watcom.                                         */
/************************************************************************/
#if defined (__WATCOMC__)
#ifdef __cplusplus
extern "C" {
int IupMain (int argc, char** argv); /* In C++ we have to declare the prototype */
}
#endif
#define main IupMain /* this is the trick for Watcom and MetroWerks */
#endif

/******************************************************************************
* Copyright (C) 1994-2012 Tecgraf, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#endif
