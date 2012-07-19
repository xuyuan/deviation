/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Deviation is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Deviation.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "target.h"
#define ENABLE_GUIOBJECT
#include "gui.h"

struct guiObject GUI_Array[100];
struct guiObject *objHEAD     = NULL;
struct guiObject *objTOUCHED  = NULL;
struct guiObject *objSELECTED = NULL;
static buttonAction_t button_action;
static u8 FullRedraw;

static u8 handle_buttons(u32 button, u8 flags, void*data);

const struct ImageMap image_map[] = {
    {"media/btn96_24.bmp", 96, 24, 0, 0}, /*FILE_BTN96_24 */
    {"media/btn48_24.bmp", 48, 24, 0, 0}, /*FILE_BTN48_24 */
    {"media/btn96_16.bmp", 96, 16, 0, 0}, /*FILE_BTN96_16 */
    {"media/btn64_16.bmp", 64, 16, 0, 0}, /*FILE_BTN64_16 */
    {"media/btn48_16.bmp", 48, 16, 0, 0}, /*FILE_BTN48_16 */
    {"media/btn32_16.bmp", 32, 16, 0, 0}, /*FILE_BTN32_16 */
    {"media/spin96.bmp",   96, 16, 0, 0}, /*FILE_SPIN96 */
    {"media/spin64.bmp",   64, 16, 0, 0}, /*FILE_SPIN64 */
    {"media/spin32.bmp",   32, 16, 0, 0}, /*FILE_SPIN32 */
    {"media/arrows16.bmp", 16, 16, 0, 0}, /*FILE_ARROW_16_UP */
    {"media/arrows16.bmp", 16, 16, 16, 0}, /*FILE_ARROW_16_DOWN */
    {"media/arrows16.bmp", 16, 16, 32, 0}, /*FILE_ARROW_16_RIGHT */
    {"media/arrows16.bmp", 16, 16, 48, 0}, /*FILE_ARROW_16_LEFT */
};
void connect_object(struct guiObject *obj)
{
    if (objHEAD == NULL) {
        objHEAD = obj;
    } else {
        struct guiObject *ptr = objHEAD;
        while(ptr->next)
            ptr = ptr->next;
        ptr->next = obj;
    }
}

u8 coords_in_box(struct guiBox *box, struct touch *coords)
{
    return(coords->x >= box->x && coords->x <= (box->x + box->width)
        && coords->y >= box->y && coords->y <= (box->y + box->height));
}

void GUI_DrawObject(struct guiObject *obj)
{
    switch (obj->Type) {
    case UnknownGUI: break;
    case Button:     GUI_DrawButton(obj);            break;
    case Label:      GUI_DrawLabel(obj);             break;
    case Image:      GUI_DrawImage(obj);             break;
    case Dialog:     GUI_DrawDialog(obj);            break;
    case CheckBox:   break;
    case Dropdown:   break;
    case XYGraph:    GUI_DrawXYGraph(obj);           break;
    case BarGraph:   GUI_DrawBarGraph(obj);          break;
    case TextSelect: GUI_DrawTextSelect(obj);        break;
    case Listbox:    GUI_DrawListbox(obj, 1);        break;
    case Keyboard:   GUI_DrawKeyboard(obj, NULL, 0); break;
    case Scrollbar:  GUI_DrawScrollbar(obj);         break;
    }
    if (obj == objSELECTED) {
        LCD_DrawRect(obj->box.x, obj->box.y, obj->box.width, obj->box.height, Display.select_color);
    }
    OBJ_SET_DIRTY(obj, 0);
    OBJ_SET_SHOWN(obj, 1);
}

void GUI_DrawObjects(void)
{

    struct guiObject *obj = objHEAD;
    while(obj) {
        if(! OBJ_IS_MODAL(obj) || obj->Type == Dialog)
            GUI_DrawObject(obj);
        obj = obj->next;
    }
}

void GUI_RemoveHierObjects(struct guiObject *obj)
{
    struct guiObject *parent = objHEAD;
    if(obj == objHEAD) {
        GUI_RemoveAllObjects();
        return;
    }
    while(parent && parent->next != obj)
        parent = parent->next;
    if(! parent)
        return;
    while(parent->next)
        GUI_RemoveObj(parent->next);
    FullRedraw = 1;
}

void GUI_RemoveAllObjects()
{
    while(objHEAD)
        GUI_RemoveObj(objHEAD);
    FullRedraw = 1;
}

void GUI_RemoveObj(struct guiObject *obj)
{
    switch(obj->Type) {
    case Dialog: {
        struct guiDialog *dialog = &obj->o.dialog;
        int i;
        for (i = 0; i < 4; i++)
            if (dialog->button[i])
                GUI_RemoveObj(dialog->button[i]);
        break;
    }
    case Scrollbar:
        BUTTON_UnregisterCallback(&obj->o.scrollbar.action);
        break;
    default: break;
    }
    
    if (objTOUCHED == obj)
        objTOUCHED = NULL;
    if (objSELECTED == obj)
        objSELECTED = NULL;
    OBJ_SET_USED(obj, 0);
    // Reattach linked list
    struct guiObject *prev = objHEAD;
    if (prev == obj) {
        objHEAD = obj->next;
    } else {
        while(prev) {
            if(prev->next == obj) {
                prev->next = obj->next;
                break;
            }
            prev = prev->next;
        }
    }
    FullRedraw = 1;
}

struct guiObject *GUI_GetFreeObj(void)
{
    int i;
    struct guiObject *obj;
    for (i = 0; i < 256; i++) {
        if (! OBJ_IS_USED(&GUI_Array[i])) {
            obj = &GUI_Array[i];
            obj->next = NULL;
            OBJ_SET_DISABLED(obj, 0);
            OBJ_SET_MODAL(obj, 0);
            OBJ_SET_DIRTY(obj, 1);
            OBJ_SET_SHOWN(obj, 0);
            return obj;
        }
    }
    return NULL;
}

void GUI_DrawBackground(u16 x, u16 y, u16 w, u16 h)
{
    if(w == 0 || h == 0)
        return;
    LCD_DrawWindowedImageFromFile(x, y, "media/devo8.bmp", w, h, x, y);
}

void GUI_DrawScreen(void)
{
    /*
     * First we need to draw the main background
     *  */
    GUI_DrawBackground(0, 0, 320, 240);
    /*
     * Then we need to draw any supporting GUI
     */
    GUI_DrawObjects();
    FullRedraw = 0;
}

struct guiObject *GUI_IsModal(void)
{
    struct guiObject *obj = objHEAD;
    while(obj) {
        if(obj->Type == Dialog && OBJ_IS_USED(obj))
            return obj;
        obj = obj->next;
    }
    return NULL;
}

void GUI_Redraw(struct guiObject *obj)
{
    OBJ_SET_DIRTY(obj, 1);
}

void GUI_RefreshScreen(void)
{
    struct guiObject *obj;
    if (FullRedraw) {
        GUI_DrawScreen();
        return;
    }
    if((obj = GUI_IsModal())) {
        if(OBJ_IS_DIRTY(obj)) {
            GUI_DrawObject(obj);
        }
    } else {
        obj = objHEAD;
        while(obj) {
            if(OBJ_IS_DIRTY(obj)) {
                if(OBJ_IS_TRANSPARENT(obj)) {
                    GUI_DrawBackground(obj->box.x, obj->box.y, obj->box.width, obj->box.height);
                }
                GUI_DrawObject(obj);
            }
            obj = obj->next;
        }
    }
}

void GUI_TouchRelease()
{
    if (objTOUCHED) {
        switch (objTOUCHED->Type) {
        case Button:
          {
            struct guiButton *button = &objTOUCHED->o.button;
            OBJ_SET_DIRTY(objTOUCHED, 1);
            if(button->CallBack)
                button->CallBack(objTOUCHED, button->cb_data);
            break;
          }
        case TextSelect:
            GUI_TouchTextSelect(objTOUCHED, NULL, -1);
            break;
        case Keyboard:
        {
            GUI_DrawKeyboard(objTOUCHED, NULL, 0);
            break;
        }
        case Scrollbar:
            GUI_TouchScrollbar(objTOUCHED, NULL, -1);
            break;
        default: break;
        }
        objTOUCHED = NULL;
    }
}

u8 GUI_CheckTouch(struct touch *coords, u8 long_press)
{
    u8 modalActive;
    modalActive = GUI_IsModal() ? 1 : 0;
    struct guiObject *obj = objHEAD;
    while(obj) {
        if (! OBJ_IS_DISABLED(obj)
            && ((modalActive == 0) || OBJ_IS_MODAL(obj)))
        {
            switch (obj->Type) {
            case UnknownGUI:
                break;
            case Button:
                if (coords_in_box(&obj->box, coords)) {
                    if (objTOUCHED && objTOUCHED != obj)
                        return 0;
                    objTOUCHED = obj;
                    OBJ_SET_DIRTY(obj, 1);
                    return 1;
                }
                break;
            case TextSelect:
                if(coords_in_box(&obj->box, coords)) {
                    if (objTOUCHED && objTOUCHED != obj)
                        return 0;
                    objTOUCHED = obj;
                    return GUI_TouchTextSelect(obj, coords, long_press);
                }
                break;
            case Listbox:
                if(coords_in_box(&obj->box, coords)) {
                    return GUI_TouchListbox(obj, coords, long_press);
                }
                break;
            case Dialog:
                break; /* Dialogs are handled by buttons */
            case Label:
                break;
            case Image:
                break;
            case CheckBox:
                break;
            case Dropdown:
                break;
            case XYGraph:
                if(coords_in_box(&obj->box, coords)) {
                    return GUI_TouchXYGraph(obj, coords, long_press);
                }
                break;
            case BarGraph:
                break;
            case Keyboard:
                //Note that this only works because the keyboard encompasses the whole screen
                if (objTOUCHED && objTOUCHED != obj)
                    return 0;
                objTOUCHED = obj;
                return GUI_DrawKeyboard(obj, coords, long_press);
                break;
            case Scrollbar:
                if(coords_in_box(&obj->box, coords)) {
                    objTOUCHED = obj;
                    return GUI_TouchScrollbar(obj, coords, long_press);
                }
                break;
            }
        }
        obj = obj->next;
    }
    return 0;
}

struct guiObject *GUI_GetNextSelectable(struct guiObject *origObj)
{
    struct guiObject *obj = origObj, *foundObj = NULL;

    if (! objHEAD)
        return NULL;
    obj = obj ? obj->next : objHEAD;
    while(obj) {
        if (obj->Type == Button || obj->Type == TextSelect) {
            foundObj = obj;
            break;
        }
        obj = obj->next;
    }
    if (! foundObj && origObj) {
        return GUI_GetNextSelectable(NULL);
    }
    return obj;
}
struct guiObject *GUI_GetPrevSelectable(struct guiObject *origObj)
{
    struct guiObject *obj = objHEAD, *objLast = NULL;
    if (obj == NULL)
        return GUI_GetNextSelectable(objHEAD);
    while(obj) {
        if (obj == origObj)
            break;
        if (obj->Type == Button || obj->Type == TextSelect)
            objLast = obj;
        obj = obj->next;
    }
    if (obj && ! objLast) {
        obj = obj->next;
        while(obj) {
            if (obj == origObj)
                break;
            if (obj->Type == Button || obj->Type == TextSelect)
                objLast = obj;
            obj = obj->next;
        }
    }
    if (! objLast)
        return origObj;
    return objLast;
}

void GUI_HandleButtons(u8 enable)
{
    if (! enable)
        BUTTON_UnregisterCallback(&button_action);
    else 
        BUTTON_RegisterCallback(&button_action,
                CHAN_ButtonMask(BUT_LEFT)
                | CHAN_ButtonMask(BUT_RIGHT)
                | CHAN_ButtonMask(BUT_UP)
                | CHAN_ButtonMask(BUT_DOWN)
                | CHAN_ButtonMask(BUT_ENTER)
                | CHAN_ButtonMask(BUT_EXIT),
                BUTTON_PRESS | BUTTON_RELEASE | BUTTON_LONGPRESS | BUTTON_PRIORITY,
                handle_buttons,
                NULL);
}

u8 handle_buttons(u32 button, u8 flags, void *data)
{
    (void)data;
    if (flags & BUTTON_RELEASE) {
        if (objSELECTED && objTOUCHED == objSELECTED && (
             CHAN_ButtonIsPressed(button, BUT_LEFT) ||
             CHAN_ButtonIsPressed(button, BUT_RIGHT) ||
             CHAN_ButtonIsPressed(button, BUT_ENTER)))
        {
            GUI_TouchRelease();
            return 1;
        } else if (CHAN_ButtonIsPressed(button, BUT_DOWN) ||
                   (! objSELECTED && CHAN_ButtonIsPressed(button, BUT_UP)))
        {
            struct guiObject *obj = GUI_GetNextSelectable(objSELECTED);
            if (obj && obj != objSELECTED) {
                if (objSELECTED)
                    OBJ_SET_DIRTY(objSELECTED, 1);
                objSELECTED = obj;
                OBJ_SET_DIRTY(obj, 1);
                return 1;
            }
        } else if (CHAN_ButtonIsPressed(button, BUT_UP)) {
            struct guiObject *obj = GUI_GetPrevSelectable(objSELECTED);
            if (obj && obj != objSELECTED) {
                if (objSELECTED)
                    OBJ_SET_DIRTY(objSELECTED, 1);
                objSELECTED = obj;
                OBJ_SET_DIRTY(obj, 1);
                return 1;
            }
        } else if (objSELECTED && CHAN_ButtonIsPressed(button, BUT_EXIT)) {
            OBJ_SET_DIRTY(objSELECTED, 1);
            objSELECTED = NULL;
            return 1;
        }
        return 0;
    }
    if (! objHEAD)
        return 0;

    if (CHAN_ButtonIsPressed(button, BUT_DOWN) ||
        CHAN_ButtonIsPressed(button, BUT_UP) ||
        CHAN_ButtonIsPressed(button, BUT_EXIT))
    {
        return 1;
    }
    if (objSELECTED) {
        if (CHAN_ButtonIsPressed(button, BUT_ENTER)) {
            if (! objTOUCHED || objTOUCHED == objSELECTED) {
                if (objSELECTED->Type == TextSelect) {
                    GUI_PressTextSelect(objSELECTED, BUT_ENTER, flags & BUTTON_LONGPRESS);
                    objTOUCHED = objSELECTED;
                    return 1;
                } else {
                    struct touch coords;
                    coords.x = objSELECTED->box.x + (objSELECTED->box.width >> 1);
                    coords.y = objSELECTED->box.y + (objSELECTED->box.height >> 1);
                    GUI_CheckTouch(&coords, flags & BUTTON_LONGPRESS);
                    return 1;
                }
            }
        } else if (objSELECTED->Type == TextSelect) {
            if (! objTOUCHED || objTOUCHED == objSELECTED) {
                if (CHAN_ButtonIsPressed(button, BUT_RIGHT)) {
                    GUI_PressTextSelect(objSELECTED, BUT_RIGHT, flags & BUTTON_LONGPRESS);
                    objTOUCHED = objSELECTED;
                    return 1;
                } else if (CHAN_ButtonIsPressed(button, BUT_LEFT)) {
                    GUI_PressTextSelect(objSELECTED, BUT_LEFT, flags & BUTTON_LONGPRESS);
                    objTOUCHED = objSELECTED;
                    return 1;
                }
            }
        }
        return 1;
    }
    return 0;
}

void GUI_DrawImageHelper(u16 x, u16 y, const struct ImageMap *map, u8 idx)
{
    LCD_DrawWindowedImageFromFile(x, y, map->file, map->width, map->height,
                                  map->x_off, map->y_off + idx * map->height);
}
