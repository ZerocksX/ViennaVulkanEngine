/********************************************************************************
** Form generated from reading UI file 'QTGameClientNYrfmN.ui'
**
** Created by: Qt User Interface Compiler version 5.15.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef QTGAMECLIENTNYRFMN_H
#define QTGAMECLIENTNYRFMN_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_QTGameClientClass
{
public:
    QAction *actionE_xit;
    QAction *actionAbout;
    QWidget *centralWidget;
    QGridLayout *gridLayout;
    QLabel *imageLabel;
    QMenuBar *menuBar;
    QMenu *menu_File;
    QMenu *menuHelp;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *QTGameClientClass)
    {
        if (QTGameClientClass->objectName().isEmpty())
            QTGameClientClass->setObjectName(QString::fromUtf8("QTGameClientClass"));
        QTGameClientClass->resize(1329, 800);
        actionE_xit = new QAction(QTGameClientClass);
        actionE_xit->setObjectName(QString::fromUtf8("actionE_xit"));
        actionAbout = new QAction(QTGameClientClass);
        actionAbout->setObjectName(QString::fromUtf8("actionAbout"));
        centralWidget = new QWidget(QTGameClientClass);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        gridLayout = new QGridLayout(centralWidget);
        gridLayout->setSpacing(6);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        imageLabel = new QLabel(centralWidget);
        imageLabel->setObjectName(QString::fromUtf8("imageLabel"));
        imageLabel->setEnabled(true);
        imageLabel->setPixmap(QPixmap(QString::fromUtf8(":/QTGameClient/loading_screen.png")));
        imageLabel->setAlignment(Qt::AlignCenter);

        gridLayout->addWidget(imageLabel, 0, 0, 1, 1);

        QTGameClientClass->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(QTGameClientClass);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 1329, 26));
        menu_File = new QMenu(menuBar);
        menu_File->setObjectName(QString::fromUtf8("menu_File"));
        menuHelp = new QMenu(menuBar);
        menuHelp->setObjectName(QString::fromUtf8("menuHelp"));
        QTGameClientClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(QTGameClientClass);
        mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
        QTGameClientClass->addToolBar(Qt::TopToolBarArea, mainToolBar);
        statusBar = new QStatusBar(QTGameClientClass);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        QTGameClientClass->setStatusBar(statusBar);

        menuBar->addAction(menu_File->menuAction());
        menuBar->addAction(menuHelp->menuAction());
        menu_File->addAction(actionE_xit);
        menuHelp->addAction(actionAbout);

        retranslateUi(QTGameClientClass);
        QObject::connect(actionE_xit, SIGNAL(triggered()), QTGameClientClass, SLOT(close()));

        QMetaObject::connectSlotsByName(QTGameClientClass);
    } // setupUi

    void retranslateUi(QMainWindow *QTGameClientClass)
    {
        QTGameClientClass->setWindowTitle(QCoreApplication::translate("QTGameClientClass", "QTGameClient", nullptr));
        actionE_xit->setText(QCoreApplication::translate("QTGameClientClass", "E&xit", nullptr));
        actionAbout->setText(QCoreApplication::translate("QTGameClientClass", "About", nullptr));
        imageLabel->setText(QString());
        menu_File->setTitle(QCoreApplication::translate("QTGameClientClass", "&File", nullptr));
        menuHelp->setTitle(QCoreApplication::translate("QTGameClientClass", "Help", nullptr));
    } // retranslateUi

};

namespace Ui {
    class QTGameClientClass: public Ui_QTGameClientClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // QTGAMECLIENTNYRFMN_H
