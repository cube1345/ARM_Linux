/****************************************************************************
** Meta object code from reading C++ file 'main_window.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.11)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../main_window.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'main_window.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.11. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    QByteArrayData data[19];
    char stringdata0[216];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 0, 10), // "MainWindow"
QT_MOC_LITERAL(1, 11, 16), // "openSelectedFile"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(4, 46, 4), // "item"
QT_MOC_LITERAL(5, 51, 11), // "showGallery"
QT_MOC_LITERAL(6, 63, 9), // "showFiles"
QT_MOC_LITERAL(7, 73, 9), // "showAudio"
QT_MOC_LITERAL(8, 83, 9), // "showVideo"
QT_MOC_LITERAL(9, 93, 8), // "showText"
QT_MOC_LITERAL(10, 102, 12), // "showSettings"
QT_MOC_LITERAL(11, 115, 11), // "toggleAudio"
QT_MOC_LITERAL(12, 127, 9), // "stopAudio"
QT_MOC_LITERAL(13, 137, 14), // "setAudioVolume"
QT_MOC_LITERAL(14, 152, 6), // "volume"
QT_MOC_LITERAL(15, 159, 17), // "updateAudioStatus"
QT_MOC_LITERAL(16, 177, 11), // "toggleVideo"
QT_MOC_LITERAL(17, 189, 9), // "stopVideo"
QT_MOC_LITERAL(18, 199, 16) // "updateVideoFrame"

    },
    "MainWindow\0openSelectedFile\0\0"
    "QListWidgetItem*\0item\0showGallery\0"
    "showFiles\0showAudio\0showVideo\0showText\0"
    "showSettings\0toggleAudio\0stopAudio\0"
    "setAudioVolume\0volume\0updateAudioStatus\0"
    "toggleVideo\0stopVideo\0updateVideoFrame"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   84,    2, 0x08 /* Private */,
       5,    0,   87,    2, 0x08 /* Private */,
       6,    0,   88,    2, 0x08 /* Private */,
       7,    0,   89,    2, 0x08 /* Private */,
       8,    0,   90,    2, 0x08 /* Private */,
       9,    0,   91,    2, 0x08 /* Private */,
      10,    0,   92,    2, 0x08 /* Private */,
      11,    0,   93,    2, 0x08 /* Private */,
      12,    0,   94,    2, 0x08 /* Private */,
      13,    1,   95,    2, 0x08 /* Private */,
      15,    0,   98,    2, 0x08 /* Private */,
      16,    0,   99,    2, 0x08 /* Private */,
      17,    0,  100,    2, 0x08 /* Private */,
      18,    0,  101,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->openSelectedFile((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 1: _t->showGallery(); break;
        case 2: _t->showFiles(); break;
        case 3: _t->showAudio(); break;
        case 4: _t->showVideo(); break;
        case 5: _t->showText(); break;
        case 6: _t->showSettings(); break;
        case 7: _t->toggleAudio(); break;
        case 8: _t->stopAudio(); break;
        case 9: _t->setAudioVolume((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 10: _t->updateAudioStatus(); break;
        case 11: _t->toggleVideo(); break;
        case 12: _t->stopVideo(); break;
        case 13: _t->updateVideoFrame(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.data,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 14;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
