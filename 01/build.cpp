 /**************************************************************************
** This file is part of AiNeSim
**
** Copyright (c) 2016 AiNeSim Team. All rights reserved.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
**
** In addition, as a special exception,  that plugins developed for AiNeSim,
** are allowed to remain closed sourced and can be distributed under any license .
** These rights are included in the file LGPL_EXCEPTION.txt in this package.
**
**************************************************************************/
// Module: ainesimbuild.cpp
// Creator: meiping peng <meipingp813@163.com>

#include "ainesimbuild.h"
#include "ainesimbuild_global.h"
#include "buildmanager.h"
#include "fileutil/fileutil.h"
#include "processex/processex.h"
#include "textoutput/textoutput.h"
#include "buildconfigdialog.h"
#include "ainesimdebugapi/ainesimdebugapi.h"
#include "ainesimeditorapi/ainesimeditorapi.h"
#include "qtc_texteditor/basetextdocumentlayout.h"
#include "../ainesimeditor/ainesimeditor_global.h"
#include <QToolBar>
#include <QComboBox>
#include <QAction>
#include <QMenu>
#include <QDir>
#include <QFileInfo>
#include <QTextBlock>
#include <QTextCodec>
#include <QProcessEnvironment>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QLabel>
#include <QCheckBox>
#include <QToolButton>
#include <QTime>
#include <QDebug>
//ainesim_memory_check_begin
#if defined(WIN32) && defined(_MSC_VER) &&  defined(_DEBUG)
     #define _CRTDBG_MAP_ALLOC
     #include <stdlib.h>
     #include <crtdbg.h>
     #define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__ )
     #define new DEBUG_NEW
#endif

enum {
    INPUT_ACTION = 0,
    INPUT_COMMAND = 1
};

enum {
    ID_CMD = 0,
    ID_ARGS = 1,
    ID_CODEC = 2,//文本格式
    ID_MIMETYPE = 3,
    ID_TASKLIST = 4,
    ID_EDITOR = 5,
    ID_INPUTTYPE = 6, //action - 0, command - 1
    ID_NAVIGATE = 7,
    ID_REGEXP = 8,
    ID_ACTIONID = 9,
    ID_TAKEALL = 10,
    ID_ACTIVATEOUTPUT_CHECK = 11
};

struct BuildBarInfo {
    BuildBarInfo() : build(0), buildMenu(0)
    {
    }
    ~BuildBarInfo()
    {
        if (buildMenu) {
            buildMenu->deleteLater();
        }
    }
    AiNeSimApi::IBuild *build;
    QMenu           *buildMenu;
    QList<QAction*>  toolbarActions;
};

AiNeSimBuild::AiNeSimBuild(AiNeSimApi::IApplication *app, QObject *parent) :
    AiNeSimApi::IAiNeSimBuild(parent),
    m_ainesimApp(app),
    m_buildManager(new BuildManager(this)),
    m_build(0)
{
    m_bDynamicBuild = true;
    m_bLockBuildRoot = false;
    if (m_buildManager->initWithApp(m_ainesimApp)) {
        m_buildManager->load(m_ainesimApp->resourcePath()+"/build");
        m_ainesimApp->extension()->addObject("AiNeSim.IBuildManager",m_buildManager);
    }    //初始化保留

    m_buildToolBar = m_ainesimApp->actionManager()->insertToolBar("toolbar/build",tr("Build Toolbar"),"toolbar/build");
    m_ainesimApp->actionManager()->insertViewMenu(AiNeSimApi::ViewMenuToolBarPos,m_buildToolBar->toggleViewAction());

    m_buildMenu = m_ainesimApp->actionManager()->loadMenu("menu/build");
    if (!m_buildMenu) {
        m_buildMenu = m_ainesimApp->actionManager()->insertMenu("menu/build",tr("&Build"),"menu/help");
    }
    /*
    m_ainesimModel = new QStandardItemModel(0,2,this);
    m_ainesimModel->setHeaderData(0,Qt::Horizontal,tr("Name"));
    m_ainesimModel->setHeaderData(1,Qt::Horizontal,tr("Value"));

    m_configModel = new QStandardItemModel(0,2,this);
    m_configModel->setHeaderData(0,Qt::Horizontal,tr("Name"));
    m_configModel->setHeaderData(1,Qt::Horizontal,tr("Value"));

    m_customModel = new QStandardItemModel(0,2,this);
    m_customModel->setHeaderData(0,Qt::Horizontal,tr("Name"));
    m_customModel->setHeaderData(1,Qt::Horizontal,tr("Value"));
    */

    AiNeSimApi::IActionContext *actionContext = m_ainesimApp->actionManager()->getActionContext(this,"Build");
    //保留活动管理

    m_configAct = new QAction(QIcon("icon:build/images/config.png"),tr("Build Configuration..."),this);
    actionContext->regAction(m_configAct,"Config","");

    m_buildToolBar->addAction(m_configAct);//不需要
    m_buildToolBar->addSeparator();//分隔符

//    m_checkBoxLockBuild = new QCheckBox;

//    m_buildToolBar->addWidget(m_checkBoxLockBuild);
//    m_buildToolBar->addSeparator();

    m_process = new ProcessEx(this);//保留
    m_output = new TextOutput(m_ainesimApp);//保留

    m_stopAct = new QAction(tr("Stop Action"),this);
    m_stopAct->setIcon(QIcon("icon:build/images/stopaction.png"));
    actionContext->regAction(m_stopAct,"Stop","");

    m_clearAct = new QAction(tr("Clear Output"),this);
    m_clearAct->setIcon(QIcon("icon:images/cleanoutput.png"));
    actionContext->regAction(m_clearAct,"ClearOutput","");

    m_ExecuteProjectAct = new QAction(tr("Build Project"),this);
    connect(m_ExecuteProjectAct,SIGNAL(triggered()),this,SLOT(ExecuteProject()));

    m_buildProjToolMenu = new QMenu("Build Project");

    m_buildAllProjToolMenu = new QMenu("Build All Project");

    m_buildProjAct = new QAction(tr("Build Project"));

    m_buildAllProjAct = new QAction(tr("Build All Project"));

    m_buildMenu->addAction(m_buildProjAct);
    m_buildMenu->addAction(m_buildAllProjAct);

    m_buildMenu->addSeparator();

    connect(m_buildProjAct,SIGNAL(triggered()),this,SLOT(fmctxBuildTool()));//主要
    connect(m_buildAllProjAct,SIGNAL(triggered()),this,SLOT(fmctxAllBuild()));//主要

    connect(m_stopAct,SIGNAL(triggered()),this,SLOT(stopAction()));//
    connect(m_clearAct,SIGNAL(triggered()),m_output,SLOT(clear()));

    m_buildMenu->addAction(m_configAct);
    m_buildMenu->addSeparator();
    m_buildMenu->addAction(m_stopAct);
    m_buildMenu->addAction(m_clearAct);
    m_buildMenu->addSeparator();

    //m_ainesimApp->outputManager()->addOutuput(m_output,tr("Build Output"));
    m_outputLineWrapAct = new QAction(tr("Line Wrap"),this);//自动换行
    m_outputLineWrapAct->setCheckable(true);
    connect(m_outputLineWrapAct,SIGNAL(toggled(bool)),this,SLOT(setOutputLineWrap(bool)));

    m_outputAutoClearAct = new QAction(tr("Auto Clear"),this);
    m_outputAutoClearAct->setCheckable(true);
    connect(m_outputAutoClearAct,SIGNAL(triggered(bool)),this,SLOT(setOutputAutoClear(bool)));

    m_outputAutoPosCursorAct = new QAction(tr("Automatic positioning cursor"),this);//自动定位光标 
    m_outputAutoPosCursorAct->setCheckable(true);
    connect(m_outputAutoPosCursorAct,SIGNAL(triggered(bool)),this,SLOT(setOutputAutoPosCursor(bool)));

    bool bLineWrap = m_ainesimApp->settings()->value(AINESIMBUILD_OUTPUTLINEWRAP,false).toBool();
    m_bOutputAutoClear = m_ainesimApp->settings()->value(AINESIMBUILD_OUTPUTAUTOCLEAR,true).toBool();

    bool bAutoPosCursor = m_ainesimApp->settings()->value(AINESIMBUILD_OUTPUTAUTOPOSCURSOR,true).toBool();

    m_output->setLineWrap(bLineWrap);
    m_outputLineWrapAct->setChecked(bLineWrap);
    m_outputAutoClearAct->setChecked(m_bOutputAutoClear);
    m_outputAutoPosCursorAct->setChecked(bAutoPosCursor);
    m_output->setAutoPosCursor(bAutoPosCursor);

    m_outputMenu = new QMenu(tr("Setup"));
    m_outputMenu->setIcon(QIcon(":/images/setup.png"));
    m_outputMenu->addAction(m_outputAutoClearAct);
    m_outputMenu->addAction(m_outputLineWrapAct);
    m_outputMenu->addAction(m_outputAutoPosCursorAct);

    m_outputAct = m_ainesimApp->toolWindowManager()->addToolWindow(Qt::BottomDockWidgetArea,
                                                                m_output,"buildoutput",
                                                                tr("Build Output"),
                                                                false,
                                                                QList<QAction*>() << m_stopAct << m_clearAct << m_outputMenu->menuAction());

    connect(m_ainesimApp,SIGNAL(loaded()),this,SLOT(appLoaded()));
    connect(m_ainesimApp->optionManager(),SIGNAL(applyOption(QString)),this,SLOT(applyOption(QString)));
    //connect(m_ainesimApp->projectManager(),SIGNAL(currentProjectChanged(AiNeSimApi::IProject*)),this,SLOT(currentProjectChanged(AiNeSimApi::IProject*)));
    connect(m_ainesimApp->editorManager(),SIGNAL(editorCreated(AiNeSimApi::IEditor*)),this,SLOT(editorCreated(AiNeSimApi::IEditor*)));
    connect(m_ainesimApp->editorManager(),SIGNAL(currentEditorChanged(AiNeSimApi::IEditor*)),this,SLOT(currentEditorChanged(AiNeSimApi::IEditor*)));
    connect(m_process,SIGNAL(extOutput(QByteArray,bool)),this,SLOT(extOutput(QByteArray,bool)));
    connect(m_process,SIGNAL(extFinish(bool,int,QString)),this,SLOT(extFinish(bool,int,QString)));
    connect(m_output,SIGNAL(dbclickEvent(QTextCursor)),this,SLOT(dbclickBuildOutput(QTextCursor)));
    connect(m_output,SIGNAL(enterText(QString)),this,SLOT(enterTextBuildOutput(QString)));
    connect(m_configAct,SIGNAL(triggered()),this,SLOT(config()));
    connect(m_ainesimApp->fileManager(),SIGNAL(aboutToShowFolderContextMenu(QMenu*,AiNeSimApi::FILESYSTEM_CONTEXT_FLAG,QFileInfo)),this,SLOT(aboutToShowFolderContextMenu(QMenu*,AiNeSimApi::FILESYSTEM_CONTEXT_FLAG,QFileInfo)));
    connect(m_checkBoxLockBuild,SIGNAL(toggled(bool)),this,SLOT(lockBuildRoot(bool)));

    m_ainesimAppInfo.insert("AINESIM_ROOT_PATH",m_ainesimApp->rootPath());
    m_ainesimAppInfo.insert("AINESIM_APP_PATH",m_ainesimApp->applicationPath());
    m_ainesimAppInfo.insert("AINESIM_RES_PATH",m_ainesimApp->resourcePath());
    m_ainesimAppInfo.insert("AINESIM_PLUGIN_PATH",m_ainesimApp->pluginPath());
    m_ainesimAppInfo.insert("AINESIM_TOOL_PATH",m_ainesimApp->toolPath());

    m_ainesimApp->extension()->addObject("AiNeSimApi.AiNeSimBuild",this);

//    m_envManager = AiNeSimApi::getEnvManager(m_ainesimApp);
//    if (m_envManager) {
//        connect(m_envManager,SIGNAL(currentEnvChanged(AiNeSimApi::IEnv*)),this,SLOT(currentEnvChanged(AiNeSimApi::IEnv*)));
//    }
    applyOption(OPTION_AINESIMEDITOR);//保留，显示到文本框
}

AiNeSimBuild::~AiNeSimBuild()
{
    qDeleteAll(m_buildBarInfoMap);
    stopAction();
    delete m_output;
    delete m_outputMenu;
}
                                        //存放编译参数            
//bool AiNeSimBuild::execBuildCommand(const QStringList &args, const QString &workDir, bool waitFinish)//保留 
//{
//    if (m_process->isRunning()) {
//        if (!m_process->waitForFinished(1000)) {
//            m_process->kill();
//        }
//        m_process->waitForFinished(100);
//    }
//    m_process->setWorkingDirectory(workDir);
//    QString buildcmd = AiNeSimApi::getToolPath(m_ainesimApp)+"/bin/mingw32-make.exe";
//    if (buildcmd.isEmpty()) {
//        return false;
//    }
//    this->execCommand(buildcmd,args.join(" "),workDir);
//    if (!waitFinish) {
//        return true;
//    }
//    if (!m_process->waitForFinished(30000)) {
//        m_process->kill();
//        return false;
//    }
//    return m_process->exitCode() == 0;
//}


//QString AiNeSimBuild::envToValue(const QString &value,QMap<QString,QString> &ainesimEnv,const QString &env)
//{
//    QString v = value;
//    QMapIterator<QString,QString> i(ainesimEnv);
//    while(i.hasNext()) {
//        i.next();
//        v.replace("$("+i.key()+")",i.value());
//    }
//    QRegExp rx("\\$\\((\\w+)\\)");
//    int pos = 0;
//    QStringList list;
//    while ((pos = rx.indexIn(v, pos)) != -1) {
//         list << rx.cap(1);
//         pos += rx.matchedLength();
//    }

////    foreach (QString str, list) {
////         if (env.contains(str)) {
////            v.replace("$("+str+")",env.value(str));
////        }
////    }
//    return v;
//}

QString AiNeSimBuild::buildTag() const
{
    return m_buildRootPath;
}

//AiNeSimApi::IBuildManager *AiNeSimBuild::buildManager() const
//{
//    return m_buildManager;
//}

void AiNeSimBuild::appendOutput(const QString &str, const QBrush &brush, bool active, bool updateExistsTextColor)
{
    if (updateExistsTextColor) {
        m_output->updateExistsTextColor();
    }
    if (active) {
        m_outputAct->setChecked(true);
    }
    m_output->append(str,brush);//brush 加到 str
}

void AiNeSimBuild::appLoaded()
{
}

void AiNeSimBuild::debugBefore()
{
}

//void AiNeSimBuild::config()
//{
//    if (!m_build) {
//        return;
//    }

//    BuildConfigDialog dlg;
//    //在XML文件中配置
//    dlg.setBuild(m_build->id(),m_buildRootPath);
//    dlg.setModel(m_ainesimModel,m_configModel,m_customModel);
//    if (dlg.exec() == QDialog::Accepted) {
//        QString key;
//        if (!m_buildRootPath.isEmpty()) {
//            key = "build-custom/"+m_buildRootPath;
//        }
//        for (int i = 0; i < m_customModel->rowCount(); i++) {
//            QStandardItem *name = m_customModel->item(i,0);
//            QStandardItem *value = m_customModel->item(i,1);
//            //m_customMap.insert(name->text(),value->text());
//            QString id = name->data().toString();
//            if (!key.isEmpty()) {
//                m_ainesimApp->settings()->setValue(key+"#"+id,value->text());
//            }
//        }
//    }
//}
//显示下拉菜单
//void AiNeSimBuild::aboutToShowFolderContextMenu(QMenu *menu, AiNeSimApi::FILESYSTEM_CONTEXT_FLAG flag, const QFileInfo &info)
//{
//    m_fmctxInfo = info;
//    if (flag == AiNeSimApi::FILESYSTEM_FILES) {
//        QString cmd = FileUtil::lookPathInDir(info.fileName(),info.path());
//        if (!cmd.isEmpty()) {
//            QAction *act = 0;
//            if (!menu->actions().isEmpty()) {
//                act = menu->actions().at(0);
//            }
////            menu->insertAction(act,m_fmctxExecuteFileAct);
////            menu->insertSeparator(act);
//        }
//    } else if (flag == AiNeSimApi::FILESYSTEM_FOLDER || flag == AiNeSimApi::FILESYSTEM_ROOTFOLDER) {
//        bool hasBuild = false;
//        bool hasTest = false;
//        foreach(QFileInfo info, QDir(info.filePath()).entryInfoList(QDir::Files)) {
//            //判断该工程目录下是否为一个工程或有makefile文件
//            if (info.fileName().endsWith("_test.cmake")) {
//                hasBuild = true;
//                hasTest = true;
//                break;
//            }
//            if (info.suffix() == "cmake") {
//                hasBuild = true;
//            }
//            if(info.fileName() == "makefile"){
//                hasBuild = true;
//            }
//        }
//        if (hasBuild) {
//            QAction *act = 0;
//            if (!menu->actions().isEmpty()) {
//                act = menu->actions().at(0);
//            }
//            menu->insertAction(act,m_buildProjAct);
//            menu->insertSeparator(act);            
//            //m_fmctxGoTestAct->setEnabled(hasTest);
////            menu->insertMenu(act,m_fmctxGoToolMenu);
////            menu->insertSeparator(act);
//        }
//    }
//}

void AiNeSimBuild::ExecuteProject()
{
    QString cmd = FileUtil::lookPathInDir(m_fmctxInfo.fileName(),m_fmctxInfo.path());
    if (!cmd.isEmpty()) {
        this->stopAction();
        this->execCommand(cmd,QString(),m_fmctxInfo.path(),true,true,false);
    }
}

//void AiNeSimBuild::fmctxGoLockBuild()
//{
//    QString buildPath = m_fmctxInfo.filePath();
//    this->lockBuildRootByMimeType(buildPath,"text/x-cmake");
//}

void AiNeSimBuild::fmctxBuildTool()//执行单个工程
{
    QAction *act = (QAction*)sender();
    if (!act) {
        return;
    }
    // build install test clean
    QString args = act->data().toString();
//    QString cmd = FileUtil::lookupGoBin("go",m_ainesimApp,false);
    QString cmd = AiNeSimApi::getToolPath(m_ainesimApp)+"/bin/mingw32-make.exe";
    m_outputRegex = "(\\w?:?[\\w\\d_\\-\\\\/\\.]+):(\\d+):";
    m_process->setUserData(ID_REGEXP,m_outputRegex);
    if (!cmd.isEmpty()) {
        m_ainesimApp->editorManager()->saveAllEditors();
        this->stopAction();
        this->execCommand(cmd,args,m_fmctxInfo.filePath(),true,true,true,false);
    }
}

//void AiNeSimBuild::fmctxBuildfmt()
//{
//    QString args = "g++ -o ";
////    QString cmd = AiNeSimApi::getGotools(m_ainesimApp);
//    QString cmd;
//    m_outputRegex = "(\\w?:?[\\w\\d_\\-\\\\/\\.]+):(\\d+):";
//    m_process->setUserData(ID_REGEXP,m_outputRegex);
//    if (!cmd.isEmpty()) {
//        m_ainesimApp->editorManager()->saveAllEditors();
//        this->stopAction();
//        this->execCommand(cmd,args,m_fmctxInfo.filePath(),true,true,true,false);
//    }
//}

void AiNeSimBuild::applyOption(QString /*opt*/)
{
//    if (opt == OPTION_AINESIMEDITOR) {
//        QFont font = m_output->font();
//        font.setFamily(m_ainesimApp->settings()->value(EDITOR_FAMILY,font.family()).toString());
//        m_output->setFont(font);
//    }
}

//void AiNeSimBuild::lockBuildRoot(bool b)//锁定编译目标
//{
//    m_bLockBuildRoot = b;
//    if (!b) {
//        this->currentEditorChanged(m_ainesimApp->editorManager()->currentEditor());
//    }
//}

void AiNeSimBuild::setOutputLineWrap(bool b)
{
    m_output->setLineWrap(b);
    m_ainesimApp->settings()->setValue(AINESIMBUILD_OUTPUTLINEWRAP,b);
}

void AiNeSimBuild::setOutputAutoClear(bool b)
{
    m_bOutputAutoClear = b;
    m_ainesimApp->settings()->setValue(AINESIMBUILD_OUTPUTAUTOCLEAR,b);
}

void AiNeSimBuild::setOutputAutoPosCursor(bool b)
{
    m_ainesimApp->settings()->setValue(AINESIMBUILD_OUTPUTAUTOPOSCURSOR,b);
    m_output->setAutoPosCursor(b);
}

bool AiNeSimBuild::isLockBuildRoot() const
{
    return m_bLockBuildRoot;
}

QString AiNeSimBuild::currentBuildPath() const
{
    return m_buildRootPath;
}

//void AiNeSimBuild::currentEnvChanged(AiNeSimApi::IEnv*)
//{
//    AiNeSimApi::IEnv *ienv = m_envManager->currentEnv();
//    if (!ienv) {
//        return;
//    }
//    QProcessEnvironment env =  AiNeSimApi::getGoEnvironment(m_ainesimApp);
////    if (!AiNeSimApi::hasGoEnv(env)) {
////        return;
////    }

//    m_ainesimApp->appendLog("AiNeSimBuild","go environment changed");
//    m_process->setEnvironment(env.toStringList());

//    bool b = m_ainesimApp->settings()->value(AINESIMBUILD_ENVCHECK,true).toBool();
//    if (!b) {
//        return;
//    }

//    QString gobin = FileUtil::lookupGoBin("go",m_ainesimApp,true);
//    if (gobin.isEmpty()) {
//        m_output->updateExistsTextColor();
//        m_output->appendTag(tr("Current environment change id \"%1\"").arg(ienv->id())+"\n");
//        m_output->append("go bin not found!",Qt::red);
//        return;
//    }
//    if (!m_process->isRunning()) {
//        m_output->updateExistsTextColor();
//        m_output->appendTag(tr("Current environment change id \"%1\"").arg(ienv->id())+"\n");
//        this->execCommand(gobin,"env",AiNeSimApi::getGOROOT(m_ainesimApp),false,false);
//    }
//}

//void AiNeSimBuild::loadProjectInfo(const QString &filePath)
//{    
//    m_projectInfo.clear();
//    if (filePath.isEmpty()) {
//        return;
//    }
//    QFileInfo info(filePath);
//    /*
//PROJECT_NAME
//PROJECT_PATH
//PROJECT_DIR
//PROJECT_DIRNAME
//PROJECT_TARGETNAME
//PROJECT_TARGETATH
//    */
//    if (info.isDir()) {
//        m_projectInfo.insert("PROJECT_NAME",info.fileName());
//        m_projectInfo.insert("PROJECT_PATH",info.filePath());
//        m_projectInfo.insert("PROJECT_DIR",info.filePath());
//        m_projectInfo.insert("PROJECT_DIRNAME",info.fileName());
//    } else {
//        m_projectInfo.insert("PROJECT_NAME",info.fileName());
//        m_projectInfo.insert("PROJECT_PATH",info.filePath());
//        m_projectInfo.insert("PROJECT_DIR",info.path());
//        m_projectInfo.insert("PROJECT_DIRNAME",QFileInfo(info.path()).fileName());
//    }
//}

AiNeSimApi::IBuild *AiNeSimBuild::findProjectBuild(AiNeSimApi::IProject *project)//寻找编译器，没有用
    if (!project) {
        return 0;
    }
    AiNeSimApi::IBuild *build = m_buildManager->findBuild(project->mimeType());
    return build;
}

//void AiNeSimBuild::reloadProject()
//{
//    AiNeSimApi::IProject *project = (AiNeSimApi::IProject*)sender();
//    if (project) {
//        loadProjectInfo(project->filePath());
//        m_targetInfo = project->targetInfo();
//    }
//}

void AiNeSimBuild::currentProjectChanged(AiNeSimApi::IProject */*project*/)
{
    return;
//    m_buildRootPath.clear();
//    m_projectInfo.clear();
//    m_targetInfo.clear();
//    m_bProjectBuild = false;
//    if (project) {
//        connect(project,SIGNAL(reloaded()),this,SLOT(reloadProject()));
//        loadProjectInfo(project->filePath());
//        m_targetInfo = project->targetInfo();
//        m_buildRootPath = project->filePath();
//        AiNeSimApi::IBuild *build = findProjectBuild(project);
//        if (build) {
//            m_bProjectBuild = true;
//            setCurrentBuild(build);
//        } else {
//            currentEditorChanged(m_ainesimApp->editorManager()->currentEditor());
//        }
//    } else {
//        AiNeSimApi::IBuild *build = findProjectBuildByEditor(m_ainesimApp->editorManager()->currentEditor());
//        if (build) {
//            m_bProjectBuild = true;
//        }
//        setCurrentBuild(build);
//    }
}

//QMap<QString,QString> AiNeSimBuild::ainesimEnvMap() const
//{
//    QMap<QString,QString> env = m_ainesimAppInfo;
//    QMapIterator<QString,QString> p(m_projectInfo);
//    while(p.hasNext()) {
//        p.next();
//        env.insert(p.key(),p.value());
//    }
//    QMapIterator<QString,QString> b(m_buildInfo);
//    while(b.hasNext()) {
//        b.next();
//        env.insert(b.key(),b.value());
//    }
//    QMapIterator<QString,QString> e(m_editorInfo);
//    while(e.hasNext()) {
//        e.next();
//        env.insert(e.key(),e.value());
//    }
//    QMapIterator<QString,QString> t(m_targetInfo);
//    while(t.hasNext()) {
//        t.next();
//        env.insert(t.key(),t.value());
//    }
//    return env;
//}

//QString AiNeSimBuild::envValue(AiNeSimApi::IBuild *build, const QString &value)
//{
//    QString buildFilePath;
//    if (m_buildRootPath.isEmpty()) {
//        AiNeSimApi::IEditor *editor = m_ainesimApp->editorManager()->currentEditor();
//        if (editor) {
//            QString filePath = editor->filePath();
//            if (!filePath.isEmpty()) {
//                buildFilePath = QFileInfo(filePath).path();
//            }
//        }
//    }

//    QMap<QString,QString> env = buildEnvMap(build,buildFilePath);
////    QProcessEnvironment sysenv = AiNeSimApi::getGoEnvironment(m_liteApp);
//    QString sysenv = AiNeSimApi::getToolPath(m_ainesimApp);
//    return this->envToValue(value,env,sysenv);
//}

AiNeSimApi::TargetInfo AiNeSimBuild::getTargetInfo()//保留
{
    AiNeSimApi::TargetInfo info;
    if (!m_build) {
        return info;
    }
    QList<BuildTarget*> lists = m_build->targetList();
    if (!lists.isEmpty()) {
        BuildTarget *target = lists.first();

        QMap<QString,QString> env = buildEnvMap(m_build,m_buildRootPath);
//        QProcessEnvironment sysenv = AiNeSimApi::getGoEnvironment(m_ainesimApp);
        QString sysenv = AiNeSimApi::getToolPath(m_ainesimApp);
        info.cmd = this->envToValue(target->cmd(),env,sysenv);//寻找编译参数
        info.args = this->envToValue(target->args(),env,sysenv);
        info.workDir = this->envToValue(target->work(),env,sysenv);
    }
    return info;
}

//QMap<QString,QString> AiNeSimBuild::buildEnvMap(AiNeSimApi::IBuild *build, const QString &buildFilePath) const
//{
//    QMap<QString,QString> env = ainesimEnvMap();
//    if (!build) {
//        return env;
//    }
//    QString customkey;
//    if (!buildFilePath.isEmpty()) {
//        customkey = "ainesimbuild-custom/"+buildFilePath;
//    }
//    QString configkey = "ainesimbuild-config/"+build->id();
//    foreach(AiNeSimApi::BuildConfig *cf, build->configList()) {
//        QString name = cf->name();
//        QString value = cf->value();
//        if (!configkey.isEmpty()) {
//            value = m_ainesimApp->settings()->value(configkey+"#"+cf->id(),value).toString();
//        }
//        QMapIterator<QString,QString> m(env);
//        while(m.hasNext()) {
//            m.next();
//            value.replace("$("+m.key()+")",m.value());
//        }
//        env.insert(name,value);
//    }
//    foreach(AiNeSimApi::BuildCustom *cf, build->customList()) {
//        QString name = cf->name();
//        QString value = cf->value();
//        if (!customkey.isEmpty()) {
//            value = m_ainesimApp->settings()->value(customkey+"#"+cf->id(),value).toString();
//        }
//        QMapIterator<QString,QString> m(env);
//        while(m.hasNext()) {
//            m.next();
//            value.replace("$("+m.key()+")",m.value());
//        }
//        env.insert(name,value);
//    }
//    return env;
//}

QMap<QString,QString> AiNeSimBuild::buildEnvMap() const
{
    return buildEnvMap(m_build,m_buildRootPath);
    /*
    AiNeSimApi::IBuild *build = m_build;
    QString buildFilePath = m_buildFilePath;
    if (!build) {
        AiNeSimApi::IEditor *editor = m_ainesimApp->editorManager()->currentEditor();
        if (editor && !editor->filePath().isEmpty()) {
            build = m_manager->findBuild(editor->mimeType());
            buildFilePath = QFileInfo(editor->filePath()).path();
        }
    }
    return buildEnvMap(build,buildFilePath);
    */
    /*
    QMap<QString,QString> env = ainesimEnvMap();
    QMapIterator<QString,QString> i(m_configMap);
    while(i.hasNext()) {
        i.next();
        QString k = i.key();
        QString v = i.value();
        QMapIterator<QString,QString> m(env);
        while(m.hasNext()) {
            m.next();
            v.replace("$("+m.key()+")",m.value());
        }
        env.insert(k,v);
    }
    QMapIterator<QString,QString> c(m_customMap);
    while(c.hasNext()) {
        c.next();
        QString k = c.key();
        QString v = c.value();
        QMapIterator<QString,QString> m(env);
        while(m.hasNext()) {
            m.next();
            v.replace("$("+m.key()+")",m.value());
        }
        env.insert(k,v);
    }    
    QMapIterator<QString,QString> p(m_projectInfo);
    while(p.hasNext()) {
        p.next();
        env.insert(p.key(),p.value());
    }
    QMapIterator<QString,QString> e(m_editorInfo);
    while(e.hasNext()) {
        e.next();
        env.insert(e.key(),e.value());
    }
    QMapIterator<QString,QString> t(m_targetInfo);
    while(t.hasNext()) {
        t.next();
        env.insert(t.key(),t.value());
    }
    return env;
    */
}

//void AiNeSimBuild::updateBuildConfig(IBuild *build)
//{
//    m_ainesimModel->removeRows(0,m_ainesimModel->rowCount());
//    QMapIterator<QString,QString> i(this->ainesimEnvMap());
//    while (i.hasNext()) {
//        i.next();
//        m_ainesimModel->appendRow(QList<QStandardItem*>()
//                                 << new QStandardItem(i.key())
//                                 << new QStandardItem(i.value()));
//    }
//    if (build) {
//        m_configModel->removeRows(0,m_configModel->rowCount());
//        m_customModel->removeRows(0,m_customModel->rowCount());
//        QString customkey;
//        if (!m_buildRootPath.isEmpty()) {
//            customkey = "ainesimbuild-custom/"+m_buildRootPath;
//        }
//        QString configkey = "ainesimbuild-config/"+build->id();
//        foreach(AiNeSimApi::BuildCustom *cf, build->customList()) {
//            QString name = cf->name();
//            QString value = cf->value();
//            if (!customkey.isEmpty()) {
//                value = m_ainesimApp->settings()->value(customkey+"#"+cf->id(),value).toString();
//            }
//            QStandardItem *item = new QStandardItem(name);
//            item->setData(cf->id());
//            m_customModel->appendRow(QList<QStandardItem*>()
//                                     << item
//                                     << new QStandardItem(value));
//        }
//        foreach(AiNeSimApi::BuildConfig *cf, build->configList()) {
//            QString name = cf->name();
//            QString value = cf->value();
//            if (!configkey.isEmpty()) {
//                value = m_ainesimApp->settings()->value(configkey+"#"+cf->id(),value).toString();
//            }
//            QStandardItem *item = new QStandardItem(name);
//            item->setData(cf->id());
//            m_configModel->appendRow(QList<QStandardItem*>()
//                                     << item
//                                     << new QStandardItem(value));
//        }
//    }
//}

//void AiNeSimBuild::setCurrentBuild(AiNeSimApi::IBuild *build)
//{
//    //update buildconfig
//    if (build) {
//        updateBuildConfig(build);
//    }

//    if (m_build == build) {
//         return;
//    }

//    m_build = build;
//    m_buildManager->setCurrentBuild(build);

//    m_outputRegex.clear();
//}
/*
BUILD_DIR_PATH
BUILD_DIR_NAME
BUILD_DIR_BASENAME

EDITOR_FILE_PATH
EDITOR_FILE_NAME
EDITOR_FILE_BASENAME
EDITOR_FILE_SUFFIX

EDITOR_DIR_PATH
EDITOR_DIR_NAME
EDITOR_DIR_BASENAME
*/
void AiNeSimBuild::loadEditorInfo(const QString &filePath)
{
    m_editorInfo.clear();
    if (filePath.isEmpty()) {
        return;
    }
    QFileInfo info(filePath);
    m_editorInfo.insert("EDITOR_FILE_PATH",info.filePath());
    m_editorInfo.insert("EDITOR_FILE_NAME",info.fileName());
    m_editorInfo.insert("EDITOR_FILE_BASENAME",info.baseName());
    m_editorInfo.insert("EDITOR_FILE_SUFFIX",info.suffix());
    m_editorInfo.insert("EDITOR_DIR_PATH",info.path());
    m_editorInfo.insert("EDITOR_DIR_NAME",QFileInfo(info.path()).fileName());
    m_editorInfo.insert("EDITOR_DIR_BASENAME",QFileInfo(info.path()).baseName());
}

void AiNeSimBuild::loadBuildPath(const QString &buildPath, const QString &buildName, const QString &buildInfo)
{
    m_buildInfo.clear();
    m_buildRootPath = buildPath;
    m_buildRootName = buildName;
    if (buildName.isEmpty()) {
        m_checkBoxLockBuild->setEnabled(false);
        m_checkBoxLockBuild->setText("");
        m_checkBoxLockBuild->setToolTip("");
    } else {
        m_checkBoxLockBuild->setEnabled(true);
        m_checkBoxLockBuild->setText(buildName);
        m_checkBoxLockBuild->setToolTip(QString("%1 : %2").arg(tr("Lock Build")).arg(buildInfo));
    }
    emit buildPathChanged(buildPath);
    if (buildPath.isEmpty()) {
        return;
    }
    QFileInfo info(buildPath);
    m_buildInfo.insert("BUILD_DIR_PATH",info.filePath());
    m_buildInfo.insert("BUILD_DIR_NAME",info.fileName());
    m_buildInfo.insert("BUILD_DIR_BASENAME",info.baseName());
}

void AiNeSimBuild::loadTargetInfo(AiNeSimApi::IBuild *build)
{
    m_targetInfo.clear();
    if (!build) {
        return;
    }
    QList<BuildTarget*> lists = build->targetList();//工程容器
    if (!lists.isEmpty()) {
        BuildTarget *target = lists.first();
        QString cmd = this->envValue(build,target->cmd());
        QString args = this->envValue(build,target->args());
        QString work = this->envValue(build,target->work());
        m_targetInfo.insert("TARGET_CMD",cmd);
        m_targetInfo.insert("TARGET_ARGS",args);
        m_targetInfo.insert("TARGET_WORK",work);
    }
}

AiNeSimApi::IBuild *AiNeSimBuild::findProjectBuildByEditor(IEditor *editor)//通过文件找到功臣路径
{
    m_buildRootPath.clear();
    m_projectInfo.clear();
    m_targetInfo.clear();

    if (!editor) {
        return 0;
    }
    QString filePath = editor->filePath();
    if (filePath.isEmpty()) {
        return 0;
    }
    QString workDir = QFileInfo(filePath).path();
    AiNeSimApi::IBuild *build = m_buildManager->findBuild(editor->mimeType());
    AiNeSimApi::IBuild *projectBuild = 0;
    QString projectPath;
    if (build != 0) {
        foreach (AiNeSimApi::BuildLookup *lookup,build->lookupList()) {
            QDir dir(workDir);
            for (int i = 0; i <= lookup->top(); i++) {
                QFileInfoList infos = dir.entryInfoList(QStringList() << lookup->file(),QDir::Files);
                if (infos.size() >= 1) {
                    projectBuild = m_buildManager->findBuild(lookup->mimeType());
                    if (projectBuild != 0) {
                        projectPath = infos.at(0).filePath();
                        m_buildRootPath = projectPath;
                        break;
                    }
                }
                dir.cdUp();
            }
        }
    }
    if (!projectBuild) {
        return 0;
    }
    loadProjectInfo(projectPath);
    QMap<QString,QString> targetInfo;
    if (m_ainesimApp->fileManager()->findProjectTargetInfo(projectPath,targetInfo)) {
        m_targetInfo = targetInfo;
    }
    return projectBuild;
}

//void AiNeSimBuild::setDynamicBuild()
//{
//    m_bDynamicBuild = true;
//}

void AiNeSimBuild::loadBuildType(const QString &mimeType)
{
    AiNeSimApi::IBuild *build = m_buildManager->findBuild(mimeType);
    updateBuildConfig(build);
    if (build == m_build) {
        return;
    }
    m_build = build;
    m_buildMimeType = mimeType;

    m_buildManager->setCurrentBuild(m_build);
    m_outputRegex.clear();

    QMenu *menu = 0;
    BuildBarInfo *info = m_buildBarInfoMap.value(mimeType);
    if (info) {
        menu = info->buildMenu;
    }
    if (menu) {
#if defined(Q_OS_OSX)
        // dirty trick to show the correct build menu at the first time on Mac OS X
        m_buildMenu->setEnabled(false);
#endif
        m_buildMenu->menuAction()->setMenu(menu);
    } else {
//        m_buildMenu->menuAction()->setMenu(m_nullMenu);
    }
    m_buildMenu->setEnabled(menu != 0);
    m_checkBoxLockBuild->setEnabled(m_build != 0);

    QMapIterator<QString,BuildBarInfo*> i(m_buildBarInfoMap);
    while(i.hasNext()) {
        i.next();
        bool visible = (i.key() == mimeType);
        foreach (QAction *act, i.value()->toolbarActions) {
            act->setVisible(visible);
        }
    }
}

void AiNeSimBuild::editorCreated(AiNeSimApi::IEditor *editor)
{
    if (!editor) {
        return;
    }
    IBuild *build = m_buildManager->findBuild(editor->mimeType());
    if (!build) {
        return;
    }
    if (!m_buildBarInfoMap.contains(build->mimeType())) {
        BuildBarInfo *info = new BuildBarInfo;
        QList<QAction*> actions = build->actions();
        QList<QAction*> acts;
        foreach (QAction *act, actions) {
            QMenu *subMenu = act->menu();
            if (subMenu) {
                BuildAction *ba = build->findAction(subMenu->menuAction()->objectName());
                if (ba) {
                    QToolButton *btn = new QToolButton(m_buildToolBar);
                    btn->setIcon(subMenu->menuAction()->icon());
                    btn->setText(subMenu->title());
                    btn->setMenu(subMenu);
                    if (ba->isFolder()) {
                        btn->setPopupMode(QToolButton::InstantPopup);
                    } else {
                        btn->setPopupMode(QToolButton::MenuButtonPopup);
                        btn->setDefaultAction(subMenu->menuAction());
                    }
                    QAction *cb = m_buildToolBar->addWidget(btn);
                    acts.push_back(cb);
                }
            } else {
                QToolButton *btn = new QToolButton(m_buildToolBar);
                btn->setDefaultAction(act);
                QAction *cb = m_buildToolBar->addWidget(btn);
                acts.push_back(cb);
            }
        }
        QMenu *menu = new QMenu;
        menu->addAction(m_configAct);
        menu->addSeparator();
        menu->addAction(m_stopAct);
        menu->addAction(m_clearAct);
        menu->addSeparator();

        foreach (QAction *act, actions) {
            QMenu *subMenu = act->menu();
            if (subMenu) {
                if (!menu->isEmpty())
                    menu->addSeparator();
                menu->addActions(subMenu->actions());
            } else {
                menu->addAction(act);
            }
        }
        info->build = build;
        info->toolbarActions = acts;
        info->buildMenu = menu;
        foreach (QAction *act, acts) {
            act->setVisible(false);
        }
        m_buildBarInfoMap.insert(build->mimeType(),info);
    }
}

void AiNeSimBuild::lockBuildRootByMimeType(const QString &path, const QString &mimeType)
{
    AiNeSimApi::IBuild *build = m_buildManager->findBuild(mimeType);
    if (!build) {
        return;
    }
    if (build->lock() != "dir") {
        return;
    }
    m_bLockBuildRoot = true;
    m_checkBoxLockBuild->setChecked(true);
    QString buildPath;
    QString buildName;
    QString buildInfo;
    QFileInfo info(path);
    buildPath = info.filePath();
    buildName = info.fileName();
    buildInfo = QDir::toNativeSeparators(buildPath);
    loadBuildPath(buildPath,buildName,buildInfo);
    loadBuildType(mimeType);
}

//void AiNeSimBuild::currentEditorChanged(AiNeSimApi::IEditor *editor)
//{
//    //check lock build file
//    if (m_bLockBuildRoot) {
//        if (m_build && m_build->lock() == "file") {
//            return;
//        }
//    }
//    if (editor) {
//        loadEditorInfo(editor->filePath());
//    } else {
//        loadEditorInfo("");
//    }
//    //check lock build dir
//    if (m_bLockBuildRoot) {
//        if (m_build && m_build->lock() == "dir") {
//            return;
//        }
//    }
//    QString mimeType;
//    if (editor) {
//        mimeType = editor->mimeType();
//    }
//    QString buildPath;
//    QString buildName;
//    QString buildInfo;
//    if (editor && !editor->filePath().isEmpty()) {
//        AiNeSimApi::IBuild *build = m_buildManager->findBuild(mimeType);
//        if (build) {
//            QFileInfo info(editor->filePath());
//            if (build->lock() == "dir") {
//                buildPath = info.path();
//                buildName = QFileInfo(info.path()).fileName();
//                buildInfo = QDir::toNativeSeparators(buildPath);
//            } else if (build->lock() == "file") {
//                buildName = info.fileName();
//                buildPath = info.path();
//                buildInfo = QDir::toNativeSeparators(info.filePath());
//            }
//        } else {
//            QFileInfo info(editor->filePath());
//            buildPath = info.path();
//        }
//    }
//    loadBuildPath(buildPath,buildName,buildInfo);
//    loadBuildType(mimeType);
//}

void AiNeSimBuild::extOutput(const QByteArray &data, bool bError)
{
    if (data.isEmpty()) {
        return;
    }
    //m_ainesimApp->outputManager()->setCurrentOutput(m_output);
    if (m_process->userData(ID_ACTIVATEOUTPUT_CHECK).toBool()) {
        m_outputAct->setChecked(true);
    }

    QString codecName = m_process->userData(2).toString();
    QTextCodec *codec = QTextCodec::codecForLocale();
    if (!codecName.isEmpty()) {
        codec = QTextCodec::codecForName(codecName.toLatin1());
    }
    QString msg = codec->toUnicode(data);
    m_output->append(msg);

    if (!m_process->userData(ID_NAVIGATE).toBool()) {
        return;
    }
    if (bError || m_process->userData(ID_TAKEALL).toBool() ) {
        QString regexp = m_process->userData(ID_REGEXP).toString();
        if (regexp.isEmpty()) {
            return;
        }
        QRegExp re(regexp);
        foreach (QString info, msg.split("\n",QString::SkipEmptyParts)) {
            if (re.indexIn(info) >= 0 && re.captureCount() >= 2) {
                QString fileName = re.cap(1);
                QString fileLine = re.cap(2);

                bool ok = false;
                int line = fileLine.toInt(&ok);
                if (ok) {
                    QDir dir(m_workDir);
                    QString filePath = dir.filePath(fileName);
                    if (QFile::exists(filePath)) {
                        fileName = filePath;
                    } else {
                        foreach(QFileInfo info,dir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
                            QString filePath = info.absoluteDir().filePath(fileName);
                            if (QFile::exists(filePath)) {
                                fileName = filePath;
                                break;
                            }
                        }
                    }

                    AiNeSimApi::IEditor *editor = m_ainesimApp->editorManager()->findEditor(fileName,true);
                    if (editor) {
                        AiNeSimApi::IAiNeSimEditor *ainesimEditor = AiNeSimApi::getAiNeSimEditor(editor);
                        if (ainesimEditor) {
                            QString str = m_process->userData(ID_ACTIONID).toString();
                            if (bError) {
                                str += " Error";
                                ainesimEditor->setNavigateHead(AiNeSimApi::EditorNavigateError,str);
                                ainesimEditor->insertNavigateMark(line-1,AiNeSimApi::EditorNavigateError,info, AINESIMBUILD_TAG);
                            } else {
                                str += " Export";
                                ainesimEditor->setNavigateHead(AiNeSimApi::EditorNavigateWarning,str);
                                ainesimEditor->insertNavigateMark(line-1,AiNeSimApi::EditorNavigateWarning,info, AINESIMBUILD_TAG);
                            }
                        }
                    }
                }
            }
        }

    }
}

void AiNeSimBuild::extFinish(bool error,int exitCode, QString msg)
{
    m_output->setReadOnly(true);

    bool isCommand = m_process->userData(ID_INPUTTYPE).toInt() == INPUT_COMMAND;

    if (!isCommand && exitCode != 0) {
        error = true;
    }

    if (error) {
        m_output->appendTag(tr("Error: %1.").arg(msg)+"\n",true);
    } else {
        if (isCommand) {
            m_output->appendTag(tr("Command exited with code %1.").arg(exitCode)+"\n");
        } else {
            m_output->appendTag(tr("Success: %1.").arg(msg)+"\n");
        }
    }

    if (!error) {
        QStringList task = m_process->userData(ID_TASKLIST).toStringList();
        if (!task.isEmpty()) {
            QString id = task.takeFirst();
            QString mime = m_process->userData(ID_MIMETYPE).toString();
            m_process->setUserData(ID_TASKLIST,task);
            execAction(mime,id);
        }
    } else {
        m_process->setUserData(ID_TASKLIST,QStringList());
    }    
}

void AiNeSimBuild::stopAction()
{
    if (m_process->isRunning()) {
        if (!m_process->waitForFinished(100)) {
            m_process->kill();
        }
        m_process->waitForFinished(100);
    }
}
//待续，添加
void AiNeSimBuild::execCommand(const QString &cmd1, const QString &args, const QString &workDir, bool updateExistsTextColor, bool activateOutputCheck, bool navigate, bool command)
{
    if (updateExistsTextColor) {
        m_output->updateExistsTextColor();
    }
    if (activateOutputCheck) {
        m_outputAct->setChecked(activateOutputCheck);
    }
    if (m_process->isRunning()) {
        m_output->append(tr("A process is currently running.  Stop the current action first.")+"\n",Qt::red);
        return;
    }
//    QProcessEnvironment sysenv = AiNeSimApi::getGoEnvironment(m_ainesimApp);
    QString path = AiNeSimApi::getToolPath(m_ainesimApp);
    QStringList sysenv;
    sysenv.append(path);
    QString cmd = cmd1.trimmed();
    m_output->setReadOnly(false);
    m_process->setEnvironment(sysenv);
    m_process->setUserData(ID_CMD,cmd);
    m_process->setUserData(ID_ARGS,args);
    m_process->setUserData(ID_CODEC,"utf-8");
    m_process->setUserData(ID_INPUTTYPE,command ? INPUT_COMMAND: INPUT_ACTION);
    m_process->setUserData(ID_NAVIGATE,navigate);
    m_process->setUserData(ID_ACTIVATEOUTPUT_CHECK,activateOutputCheck);
//    QString shell = FileUtil::lookPathInDir(cmd,workDir);
//    if (shell.isEmpty()) {
//        shell = FileUtil::lookPath(cmd,sysenv,false);
//    }
//    if (!shell.isEmpty()) {
//        cmd = shell;
//    }
    m_workDir = workDir;
    m_process->setWorkingDirectory(workDir);
    m_output->appendTag(QString("%1 %2 [%3]\n")
                         .arg(cmd).arg(args).arg(workDir));
#ifdef Q_OS_WIN
    m_process->setNativeArguments(args);
    if (cmd.indexOf(" ")) {
        m_process->start("\""+cmd+"\"");
    } else {
        m_process->start(cmd);
    }
#else
    m_process->start(cmd+" "+args);
#endif
}

void AiNeSimBuild::buildAction(AiNeSimApi::IBuild* build,AiNeSimApi::BuildAction* ba)
{  
    if (m_bOutputAutoClear) {
        m_output->clear();
    } else {
        m_output->updateExistsTextColor(true);
    }
    m_outputAct->setChecked(true);
    if (m_process->isRunning()) {        
        if (ba->isKillOld()) {
            m_output->append(tr("Killing current process...")+"\n");
            m_process->kill();
            if (!m_process->waitForFinished(1000)) {
                m_output->append(tr("Failed to terminate the existing process!")+"\n",Qt::red);
                return;
            }
        } else {
            m_output->append(tr("A process is currently running.  Stop the current action first.")+"\n",Qt::red);
            return;
        }
    }

    QString mime = build->mimeType();
    QString id = ba->id();
    QString editor;
    AiNeSimApi::IEditor *ed = m_ainesimApp->editorManager()->currentEditor();
    if (ed) {
        editor = ed->filePath();
    }

    m_output->updateExistsTextColor();
    m_process->setUserData(ID_MIMETYPE,mime);
    m_process->setUserData(ID_EDITOR,editor);
    m_process->setUserData(ID_ACTIVATEOUTPUT_CHECK,true);
    if (ba->task().isEmpty()) {
        execAction(mime,id);
    } else {
        QStringList task = ba->task();
        QString id = task.takeFirst();
        m_process->setUserData(ID_TASKLIST,task);
        execAction(mime,id);
    }
}

void AiNeSimBuild::execAction(const QString &mime, const QString &id)
{
    if (m_process->isRunning()) {
        return;
    }

    AiNeSimApi::IBuild *build = m_buildManager->findBuild(mime);
    if (!build) {
        return;
    }

    AiNeSimApi::BuildAction *ba = build->findAction(id);
    if (!ba) {
        return;
    }

    QString codec = ba->codec();
    AiNeSimApi::IEditor *editor = m_ainesimApp->editorManager()->currentEditor();
    if (ba->save() == "project") {
        if (editor && editor->isModified()) {
            m_ainesimApp->editorManager()->saveEditor();
        }
        m_ainesimApp->projectManager()->saveProject();
    } else if(ba->save() == "editor") {
        if (editor && editor->isModified()) {
            m_ainesimApp->editorManager()->saveEditor();
        }
    } else if (ba->save() == "all") {
        m_ainesimApp->editorManager()->saveAllEditors();
    }

    QString editorPath = m_process->userData(ID_EDITOR).toString();
    QString buildFilePath;
    if (!editorPath.isEmpty() && !m_bLockBuildRoot) {
        buildFilePath = QFileInfo(editorPath).path();
    } else {
        buildFilePath = m_buildRootPath;
    }

    QMap<QString,QString> env = buildEnvMap(build,buildFilePath);

//    QProcessEnvironment sysenv = AiNeSimApi::getGoEnvironment(m_ainesimApp);
    QString sysenv = AiNeSimApi::getToolPath(m_ainesimApp);
    QString cmd = this->envToValue(ba->cmd(),env,sysenv);
    QString args = this->envToValue(ba->args(),env,sysenv);

    m_workDir = this->envToValue(build->work(),env,sysenv);
    QString work = ba->work();
    if (!work.isEmpty()) {
        m_workDir = this->envToValue(work,env,sysenv);
    }

//    if (!QFileInfo(cmd).exists()) {
//        QString findCmd = FileUtil::lookPathInDir(cmd,m_workDir);
//        if (!findCmd.isEmpty()) {
//            cmd = findCmd;
//        }
//    }
//    QString shell;
//    if (ba->cmd() == "$(GO)") {
//        shell = FileUtil::lookupGoBin(cmd,m_ainesimApp,false);
//    } else {
//        shell = FileUtil::lookPathInDir(cmd,m_workDir);
//    }
//    if (shell.isEmpty()) {
//        shell = FileUtil::lookPath(cmd,sysenv,false);
//    }
//    if (!shell.isEmpty()) {
//        cmd = shell;
//    }

    if (cmd.indexOf("$(") >= 0 || args.indexOf("$(") >= 0 || m_workDir.isEmpty()) {
        m_output->appendTag(tr("> Could not parse action '%1'").arg(ba->id())+"\n");
        m_process->setUserData(ID_TASKLIST,QStringList());
        return;
    }


    if (!ba->regex().isEmpty()) {
        m_outputRegex = this->envToValue(ba->regex(),env,sysenv);
        m_process->setUserData(ID_REGEXP,m_outputRegex);
    } else {
        m_process->setUserData(ID_REGEXP,"");
    }

    if (ba->isOutput() && ba->isReadline()) {
        m_output->setReadOnly(false);
    } else {
        m_output->setReadOnly(true);
    }

//    if (ba->func() == "debug") {
//        AiNeSimApi::IAiNeSimDebug *debug = AiNeSimApi::getAiNeSimDebug(m_ainesimApp);
//        if (debug) {
//            debug->startDebug(cmd,args,work);
//        }
//        return;
//    }
    QStringList syspath;
    syspath.append(sysenv);
    m_process->setEnvironment(syspath);
    m_process->setUserData(ID_NAVIGATE,ba->isNavigate());
    m_process->setUserData(ID_ACTIONID,ba->id());
    m_process->setUserData(ID_TAKEALL,ba->isTakeall());

    if (ba->isNavigate()) {
        foreach(AiNeSimApi::IEditor *editor, m_ainesimApp->editorManager()->editorList()) {
            AiNeSimApi::IAiNeSimEditor *ainesimEditor = AiNeSimApi::getAiNeSimEditor(editor);
            if (ainesimEditor) {
                ainesimEditor->clearAllNavigateMark(AiNeSimApi::EditorNavigateBad);
                ainesimEditor->setNavigateHead(AiNeSimApi::EditorNavigateNormal,"Normal");
            }
        }
    }

    args = args.trimmed();

    if (!ba->isOutput()) {
        bool b = QProcess::startDetached(cmd,args.split(" "),m_workDir);
        m_output->appendTag(QString("%1 %2 [%3]\n")
                             .arg(QDir::cleanPath(cmd)).arg(args).arg(m_workDir));
        m_output->appendTag(b?tr("Started process successfully"):tr("Failed to start process")+"\n");
    } else {
        m_process->setUserData(ID_CMD,cmd);
        m_process->setUserData(ID_ARGS,args);
        m_process->setUserData(ID_CODEC,codec);
        m_process->setUserData(ID_INPUTTYPE,INPUT_ACTION);

        m_process->setWorkingDirectory(m_workDir);        
        m_output->appendTag(QString("%1 %2 [%3]\n")
                            .arg(QDir::cleanPath(cmd))
                            .arg(args)
                            .arg(m_workDir));
#ifdef Q_OS_WIN
        m_process->setNativeArguments(args);
        m_process->start("\""+cmd+"\"");
#else
        m_process->start(cmd + " " + args);
#endif
    }
}

void AiNeSimBuild::enterTextBuildOutput(QString text)
{
    if (!m_process->isRunning()) {
        return;
    }
    QTextCodec *codec = QTextCodec::codecForLocale();
    QString codecName = m_process->userData(2).toString();
    if (!codecName.isEmpty()) {
        codec = QTextCodec::codecForName(codecName.toLatin1());
    }
    if (codec) {
        m_process->write(codec->fromUnicode(text));
    } else {
        m_process->write(text.toLatin1());
    }
}

void AiNeSimBuild::dbclickBuildOutput(const QTextCursor &cur)
{
    if (m_outputRegex.isEmpty()) {
        //m_outputRegex = "([\\w\\d_\\\\/\\.]+):(\\d+):";
        m_outputRegex = "(\\w?:?[\\w\\d_\\-\\\\/\\.]+):(\\d+):";
    }
    QRegExp rep(m_outputRegex);//"([\\w\\d:_\\\\/\\.]+):(\\d+)");

    int index = rep.indexIn(cur.block().text());
    if (index < 0)
        return;
    QStringList capList = rep.capturedTexts();

    if (capList.count() < 3)
        return;
    QString fileName = capList[1];
    QString fileLine = capList[2];

    bool ok = false;
    int line = fileLine.toInt(&ok);
    if (!ok)
        return;

    QDir dir(m_workDir);
    QString filePath = dir.filePath(fileName);
    if (QFile::exists(filePath)) {
        fileName = filePath;
    } else {
        foreach(QFileInfo info,dir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
            QString filePath = info.absoluteDir().filePath(fileName);
            if (QFile::exists(filePath)) {
                fileName = filePath;
                break;
            }
        }
    }

    if (AiNeSimApi::gotoLine(m_ainesimApp,fileName,line-1,0,true,true)) {
        QTextCursor lineCur = cur;
        lineCur.select(QTextCursor::LineUnderCursor);
        m_output->setTextCursor(lineCur);
    }
//    return;
//    AiNeSimApi::IEditor *editor = m_ainesimApp->fileManager()->openEditor(fileName);
//    if (editor) {
//        QTextCursor lineCur = cur;
//        lineCur.select(QTextCursor::LineUnderCursor);
//        m_output->setTextCursor(lineCur);
//        editor->widget()->setFocus();
//        AiNeSimApi::ITextEditor *textEditor = AiNeSimApi::findExtensionObject<AiNeSimApi::ITextEditor*>(editor,"AiNeSimApi.ITextEditor");
//        if (textEditor) {
//            textEditor->gotoLine(line-1,0,true);
//        }
//    }
}
