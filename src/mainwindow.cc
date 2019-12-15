/*
 * Copyright (C) 2019 Ashar Khan <ashar786khan@gmail.com>
 *
 * This file is part of CPEditor.
 *
 * CPEditor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * I will not be responsible if CPEditor behaves in unexpected way and
 * causes your ratings to go down and or loose any important contest.
 *
 * Believe Software is "Software" and it isn't not immune to bugs.
 *
 */

#include "mainwindow.hpp"

#include <Core.hpp>
#include <DiffViewer.hpp>
#include <MessageLogger.hpp>
#include <QCXXHighlighter>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPythonCompleter>
#include <QPythonHighlighter>
#include <QSyntaxStyle>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <expand.hpp>

#include "../ui/ui_mainwindow.h"

// ***************************** RAII  ****************************
MainWindow::MainWindow(QString filePath, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  setEditor();
  setLogger();
  setSettingsManager();
  restoreSettings();
  runEditorDiagonistics();
  setupCore();
  runner->removeExecutable();
  launchSession(false);
  checkUpdates();

  if (!filePath.isEmpty()) {
    openFile = new QFile(filePath);
    openFile->open(QIODevice::ReadWrite | QFile::Text);
    if (openFile->isOpen()) {
      this->window()->setWindowTitle("CP Editor : " + openFile->fileName());
      editor->setPlainText(openFile->readAll());
    } else {
      Log::MessageLogger::warn(
          "Loader",
          "The filepath was not loaded. Read/Write permission missing");
      openFile->close();
      delete openFile;
      openFile = nullptr;
    }
  }
}

MainWindow::~MainWindow() {
  delete ui;
  delete editor;
  delete setting;
  if (openFile != nullptr && openFile->isOpen())
    openFile->close();
  delete openFile;
  delete inputReader;
  delete outputReader;
  delete outputWriter;
  delete formatter;
  delete compiler;
  delete runner;
  delete saveTimer;
  delete updater;

  delete expected1;
  delete expected2;
  delete expected3;
}

// ************************* RAII HELPER *****************************

void MainWindow::setEditor() {
  editor = new QCodeEditor();
  editor->setMinimumWidth(700);
  editor->setMinimumHeight(400);

  editor->setSyntaxStyle(QSyntaxStyle::defaultStyle());  // default is white
  editor->setHighlighter(new QCXXHighlighter);
  editor->setAutoIndentation(true);
  editor->setAutoParentheses(true);
  editor->setWordWrapMode(QTextOption::NoWrap);

  ui->verticalLayout_8->addWidget(editor);

  ui->in1->setWordWrapMode(QTextOption::NoWrap);
  ui->in2->setWordWrapMode(QTextOption::NoWrap);
  ui->in3->setWordWrapMode(QTextOption::NoWrap);
  ui->out1->setWordWrapMode(QTextOption::NoWrap);
  ui->out2->setWordWrapMode(QTextOption::NoWrap);
  ui->out3->setWordWrapMode(QTextOption::NoWrap);

  saveTimer = new QTimer();
  saveTimer->setSingleShot(false);
  saveTimer->setInterval(10000);  // every 10 sec save

  expected1 = new QString;
  expected2 = new QString;
  expected3 = new QString;

  QObject::connect(editor, SIGNAL(textChanged()),
                   this, SLOT(on_textChanged_triggered()));
}

void MainWindow::setLogger() {
  Log::MessageLogger::setContainer(ui->compiler_edit);
  ui->compiler_edit->setWordWrapMode(QTextOption::NoWrap);
  ui->compiler_edit->setReadOnly(true);
}

void MainWindow::setSettingsManager() {
  setting = new Settings::SettingManager();
}

void MainWindow::saveSettings() {
  setting->setDarkTheme(ui->actionDark_Theme->isChecked());
  setting->setWrapText(ui->actionWrap_Text->isChecked());
  setting->setAutoIndent(ui->actionAuto_Indentation->isChecked());
  setting->setAutoParenthesis(ui->actionAuto_Parenthesis->isChecked());
  setting->setFont(editor->font().toString().toStdString());
  setting->setAutoSave(ui->actionAuto_Save->isChecked());
  if (!this->isMaximized())
    setting->setGeometry(this->geometry());
  setting->setTabs(ui->actionUse_Tabs->isChecked());
  setting->setMaximizedWindow(this->isMaximized());
}

void MainWindow::checkUpdates() {
  if (updater != nullptr)
    updater->checkUpdate();
}

void MainWindow::restoreSettings() {
  if (setting->isDarkTheme()) {
    ui->actionDark_Theme->setChecked(true);
    on_actionDark_Theme_triggered(true);
  }
  if (setting->isWrapText()) {
    ui->actionWrap_Text->setChecked(true);
    on_actionWrap_Text_triggered(true);
  }
  if (!setting->isAutoIndent()) {
    ui->actionAuto_Indentation->setChecked(false);
    on_actionAuto_Indentation_triggered(false);
  }
  if (!setting->isAutoParenthesis()) {
    ui->actionAuto_Parenthesis->setChecked(false);
    on_actionAuto_Parenthesis_triggered(false);
  }
  auto lang = setting->getDefaultLang();

  if (lang == "Java") {
    ui->actionC_C->setChecked(false);
    ui->actionJava->setChecked(true);
    ui->actionPython->setChecked(false);
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    // TODO(Add Java Highlighter)
    language = "Java";

  } else if (lang == "Python") {
    ui->actionC_C->setChecked(false);
    ui->actionJava->setChecked(false);
    ui->actionPython->setChecked(true);

    editor->setCompleter(new QPythonCompleter);
    editor->setHighlighter(new QPythonHighlighter);
    language = "Python";
  } else {
    if (lang != "Cpp")
      Log::MessageLogger::warn(
          "Settings",
          "File was distrubed, Cannot load default language, fall back to C++");
    ui->actionC_C->setChecked(true);
    ui->actionJava->setChecked(false);
    ui->actionPython->setChecked(false);

    // TODO(Add C++ Completer)
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    language = "Cpp";
  }

  if (!setting->getFont().empty()) {
    auto font = QFont();
    if (font.fromString(QString::fromStdString(setting->getFont()))) {
      editor->setFont(font);
    }
  }
  if (setting->isAutoSave()) {
    ui->actionAuto_Save->setChecked(true);
    on_actionAuto_Save_triggered(true);
  }

  if (!setting->getGeometry().isEmpty() && !setting->getGeometry().isNull() &&
      setting->getGeometry().isValid() && !setting->isMaximizedWindow()) {
    this->setGeometry(setting->getGeometry());
  }

  if (setting->isMaximizedWindow()) {
    this->showMaximized();
  }

  ui->actionUse_Tabs->setChecked(setting->isTabs());
  editor->setTabReplace(!ui->actionUse_Tabs->isChecked());

  const int tabStop = setting->getTabStop();
  QFontMetrics metric(editor->font());
  editor->setTabReplaceSize(tabStop);
  editor->setTabStopDistance(tabStop * metric.horizontalAdvance(" "));

  if (setting->isCompetitiveCompanionActive()) {
    createAndAttachServer();
  }

  ui->actionEnable_Companion->setChecked(
      setting->isCompetitiveCompanionActive());

  ui->actionBeta_Updates->setChecked(
      setting->isBeta());
}

void MainWindow::runEditorDiagonistics() {
  if (!Core::Formatter::check(
          QString::fromStdString(setting->getFormatCommand()))) {
    Log::MessageLogger::error(
        "Formatter",
        "Format feature will not work as your format command is not valid");
  }

  if (!Core::Compiler::check(
          QString::fromStdString(setting->getCompileCommand()))) {
    Log::MessageLogger::error("Compiler",
                              "Compiler will not work, Change Compile command "
                              "and make sure it is in path");
  }

  if (setting->getTemplatePath().size() != 0 &&
      !QFile::exists(QString::fromStdString(setting->getTemplatePath()))) {
    Log::MessageLogger::error(
        "Template",
        "The specified template file does not exists or is not readable");
  }
}

void MainWindow::setupCore() {
  formatter =
      new Core::Formatter(QString::fromStdString(setting->getFormatCommand()));
  outputReader = new Core::IO::OutputReader(ui->out1, ui->out2, ui->out3);
  outputWriter = new Core::IO::OutputWriter(ui->out1, ui->out2, ui->out3);
  inputReader = new Core::IO::InputReader(ui->in1, ui->in2, ui->in3);
  compiler =
      new Core::Compiler(QString::fromStdString(setting->getCompileCommand()));
  runner =
      new Core::Runner(QString::fromStdString(setting->getRunCommand()),
                       QString::fromStdString(setting->getCompileCommand()),
                       QString::fromStdString(setting->getPrependRunCommand()));

  updater = new Telemetry::UpdateNotifier(setting->isBeta());

  QObject::connect(runner, SIGNAL(firstExecutionFinished(QString, QString)),
                   this, SLOT(firstExecutionFinished(QString, QString)));

  QObject::connect(runner, SIGNAL(secondExecutionFinished(QString, QString)),
                   this, SLOT(secondExecutionFinished(QString, QString)));

  QObject::connect(runner, SIGNAL(thirdExecutionFinished(QString, QString)),
                   this, SLOT(thirdExecutionFinished(QString, QString)));

  QObject::connect(saveTimer, SIGNAL(timeout()), this,
                   SLOT(onSaveTimerElapsed()));
}

void MainWindow::clearTests() {
  ui->in1->clear();
  ui->in2->clear();
  ui->in3->clear();

  ui->out1->clear();
  ui->out2->clear();
  ui->out3->clear();

  expected1->clear();
  expected2->clear();
  expected3->clear();

  updateVerdict(Core::Verdict::UNKNOWN, 1);
  updateVerdict(Core::Verdict::UNKNOWN, 2);
  updateVerdict(Core::Verdict::UNKNOWN, 3);
}

void MainWindow::launchSession(bool confirm) {
  if (confirm && !closeChangedConfirm())
    return;

  if (openFile != nullptr) {
    if (openFile->isOpen()) {
      openFile->close();
    }
    delete openFile;
    openFile = nullptr;
  }

  editor->clear();
  runner->removeExecutable();

  if (setting->getTemplatePath().size() != 0) {
    if (QFile::exists(QString::fromStdString(setting->getTemplatePath()))) {
      QFile f(QString::fromStdString(setting->getTemplatePath()));
      f.open(QIODevice::ReadOnly | QFile::Text);
      editor->setPlainText(f.readAll());
      Log::MessageLogger::info("Template", "Template was successfully loaded");
    } else {
      Log::MessageLogger::error("Template",
                                "Template could not be loaded from the file " +
                                    setting->getTemplatePath());
    }
  }

  this->window()->setWindowTitle("CP Editor: Temporary buffer");

  clearTests();
}

void MainWindow::updateVerdict(Core::Verdict verdict, int target) {
  QString verdict_text, style_sheet;

  switch (verdict) {
    case Core::Verdict::ACCEPTED:
      verdict_text = "Verdict : AC";
      style_sheet = "QLabel { color : rgb(0, 180, 0); }";
      break;
    case Core::Verdict::WRONG_ANSWER:
      verdict_text = "Verdict : WA";
      style_sheet = "QLabel { color : rgb(255, 0, 0); }";
      break;
    case Core::Verdict::UNKNOWN:
      verdict_text = "Verdict : **";
      style_sheet = "";
      break;
  }

  switch (target) {
    case 1:
      ui->out1_verdict->setText(verdict_text);
      ui->out1_verdict->setStyleSheet(style_sheet);
      break;
    case 2:
      ui->out2_verdict->setText(verdict_text);
      ui->out2_verdict->setStyleSheet(style_sheet);
      break;
    case 3:
      ui->out3_verdict->setText(verdict_text);
      ui->out3_verdict->setStyleSheet(style_sheet);
      break;
  }
}

void MainWindow::createAndAttachServer() {
  if (server == nullptr) {
    server = new Network::CompanionServer(setting->getConnectionPort());
    QObject::connect(server, &Network::CompanionServer::onRequestArrived, this,
                     &MainWindow::onCompanionRequest);
  }
}

void MainWindow::applyCompanion(Network::CompanionData data) {
  if (openFile == nullptr && !isTextChanged()) {
    launchSession(false);
    QString meta = data.toMetaString();
    meta.prepend("\n");
    meta.append("Powered by CP Editor (https://github.com/coder3101/cp-editor2)");

    if (language == "Python")
      meta.replace('\n', "\n# ");
    else
      meta.replace('\n', "\n// ");

    editor->setPlainText(meta + "\n\n" + editor->toPlainText());
  }

  clearTests();

  if (data.testcases.size() > 3) {
    Log::MessageLogger::warn(
        "CP Editor",
        "More than 3 testcase were produced. Only First 3 will be used");
  }

  if (data.testcases.size() >= 1) {
    ui->in1->setPlainText(data.testcases[0].input);
    expected1->operator=(data.testcases[0].output);
  }

  if (data.testcases.size() >= 2) {
    ui->in2->setPlainText(data.testcases[1].input);
    expected2->operator=(data.testcases[1].output);
  }

  if (data.testcases.size() >= 3) {
    ui->in3->setPlainText(data.testcases[2].input);
    expected3->operator=(data.testcases[2].output);
  }
}

// ******************* STATUS::ACTIONS FILE **************************
void MainWindow::on_actionNew_triggered() {
  launchSession(true);
}

void MainWindow::on_actionOpen_triggered() {
  auto fileName = QFileDialog::getOpenFileName(
      this, tr("Open File"), "",
      "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
  if (fileName.isEmpty())
    return;
  QFile* newFile = new QFile(fileName);
  newFile->open(QFile::Text | QIODevice::ReadWrite);

  if (!closeChangedConfirm())
    return;

  if (newFile->isOpen()) {
    editor->setPlainText(newFile->readAll());
    if (openFile != nullptr && openFile->isOpen())
      openFile->close();
    openFile = newFile;
    Log::MessageLogger::info("Open",
                             "Opened " + openFile->fileName().toStdString());
    this->window()->setWindowTitle("CP Editor: " + openFile->fileName());

  } else {
    Log::MessageLogger::error(
        "Open", "Cannot Open, Do I have read and write permissions?");
  }
}

void MainWindow::on_actionSave_triggered() {
  saveFile(true, "Save");
}

void MainWindow::on_actionSave_as_triggered() {
  auto filename = QFileDialog::getSaveFileName(
      this, tr("Save as File"), "",
      "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
  if (filename.isEmpty())
    return;
  QFile savedFile(filename);
  savedFile.open(QIODevice::ReadWrite | QFile::Text);
  if (savedFile.isOpen()) {
    savedFile.write(editor->toPlainText().toStdString().c_str());
    Log::MessageLogger::info(
        "Save as", "Saved new file name " + savedFile.fileName().toStdString());
  } else {
    Log::MessageLogger::error(
        "Save as",
        "Cannot Save as new file, Is write permission allowed to me?");
  }
  on_textChanged_triggered();
}

void MainWindow::on_actionAuto_Save_triggered(bool checked) {
  if (checked) {
    saveTimer->start();
  } else {
    saveTimer->stop();
  }
}

void MainWindow::on_actionQuit_triggered() {
  if (closeChangedConfirm())
    QApplication::quit();
}

void MainWindow::on_textChanged_triggered() {
  bool isChanged = isTextChanged();
  if (openFile == nullptr) {
    if (isChanged)
      this->window()->setWindowTitle("CP Editor: Temporary buffer *");
    else
      this->window()->setWindowTitle("CP Editor: Temporary buffer");
  }
  else {
    if (isChanged)
      this->window()->setWindowTitle("CP Editor : " + openFile->fileName() + " *");
    else
      this->window()->setWindowTitle("CP Editor : " + openFile->fileName());
  }
}

// *********************** ACTIONS::EDITOR ******************************
void MainWindow::on_actionDark_Theme_triggered(bool checked) {
  if (checked) {
    QFile fullDark(":/qdarkstyle/style.qss");
    fullDark.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&fullDark);

    auto qFile = new QFile(":/styles/drakula.xml");
    qFile->open(QIODevice::ReadOnly);
    auto darkTheme = new QSyntaxStyle(this);
    darkTheme->load(qFile->readAll());

    qApp->setStyleSheet(ts.readAll());
    editor->setSyntaxStyle(darkTheme);

  } else {
    qApp->setStyleSheet("");
    editor->setSyntaxStyle(QSyntaxStyle::defaultStyle());
    auto oldbackground = editor->styleSheet();
    Log::MessageLogger::info(
        "CP Editor",
        "If theme is not set correctly. Please again change theme");
  }
}

void MainWindow::on_actionWrap_Text_triggered(bool checked) {
  if (checked)
    editor->setWordWrapMode(QTextOption::WordWrap);
  else
    editor->setWordWrapMode(QTextOption::NoWrap);
}

void MainWindow::on_actionAuto_Indentation_triggered(bool checked) {
  if (checked)
    editor->setAutoIndentation(true);
  else
    editor->setAutoIndentation(false);
}

void MainWindow::on_actionUse_Tabs_triggered(bool checked) {
  editor->setTabReplace(!checked);
}

void MainWindow::on_actionAuto_Parenthesis_triggered(bool checked) {
  if (checked)
    editor->setAutoParentheses(true);
  else
    editor->setAutoParentheses(false);
}

void MainWindow::on_actionFont_triggered() {
  bool ok;
  QFont font = QFontDialog::getFont(&ok, editor->font());
  if (ok) {
    editor->setFont(font);
  }
}

// ************************************* ACTION::ABOUT ************************

void MainWindow::on_actionBeta_Updates_triggered(bool checked) {
  setting->setBeta(checked);
  updater->setBeta(checked);
  updater->checkUpdate();
}
void MainWindow::on_actionAbout_triggered() {
  QMessageBox::about(
      this,
      QString::fromStdString(std::string("About CP Editor ") +
                             APP_VERSION_MAJOR + "." + APP_VERSION_MINOR + "." +
                             APP_VERSION_PATCH),

      "<p>The <b>CP Editor</b> is a competitive programmer's editor "
      "which can ease the task of compiling, testing and running a program"
      "so that you (a great programmer) can focus fully on your algorithms "
      "designs. </p>");
}

void MainWindow::on_actionAbout_Qt_triggered() {
  QApplication::aboutQt();
}
void MainWindow::on_actionReset_Settings_triggered() {
  auto res = QMessageBox::question(this, "Reset settings?",
                                   "Are you sure you want to reset the"
                                   " settings?",
                                   QMessageBox::Yes | QMessageBox::No);
  if (res == QMessageBox::Yes) {
    setting->setFormatCommand("clang-format -i");
    setting->setCompileCommands("g++ -Wall");
    setting->setRunCommand("");
    setting->setDefaultLanguage("Cpp");
    setting->setTemplatePath("");
    setting->setPrependRunCommand("");
    setting->setTabStop(4);

    formatter->updateCommand(
        QString::fromStdString(setting->getFormatCommand()));
    compiler->updateCommand(
        QString::fromStdString(setting->getCompileCommand()));

    runner->updateRunCommand(QString::fromStdString(setting->getRunCommand()));
    runner->updateCompileCommand(
        QString::fromStdString(setting->getCompileCommand()));
    runner->updateRunStartCommand(
        QString::fromStdString(setting->getPrependRunCommand()));
    runEditorDiagonistics();
  }
}

// ********************** GLOBAL::WINDOW **********************************
void MainWindow::closeEvent(QCloseEvent* event) {
  if (closeChangedConfirm())
    event->accept();
  else
    event->ignore();
}

void MainWindow::on_compile_clicked() {
  on_actionCompile_triggered();
}

void MainWindow::on_run_clicked() {
  on_actionRun_triggered();
}

void MainWindow::on_runOnly_clicked() {
  Log::MessageLogger::clear();
  inputReader->readToFile();

  updateVerdict(Core::Verdict::UNKNOWN, 1);
  updateVerdict(Core::Verdict::UNKNOWN, 2);
  updateVerdict(Core::Verdict::UNKNOWN, 3);

  ui->out1->clear();
  ui->out2->clear();
  ui->out3->clear();

  runner->run(!ui->in1->toPlainText().trimmed().isEmpty(),
              !ui->in2->toPlainText().trimmed().isEmpty(),
              !ui->in3->toPlainText().trimmed().isEmpty(), language);
}

void MainWindow::on_actionDetached_Execution_triggered() {
  Log::MessageLogger::clear();
  runner->runDetached(editor, language);
}

// ************************ ACTIONS::ACTIONS ******************

void MainWindow::on_actionFormat_triggered() {
  formatter->format(editor);
}

void MainWindow::on_actionRun_triggered() {
  Log::MessageLogger::clear();

  updateVerdict(Core::Verdict::UNKNOWN, 1);
  updateVerdict(Core::Verdict::UNKNOWN, 2);
  updateVerdict(Core::Verdict::UNKNOWN, 3);

  saveFile(false, "Compiler");

  ui->out1->clear();
  ui->out2->clear();
  ui->out3->clear();
  inputReader->readToFile();
  runner->run(editor, !ui->in1->toPlainText().trimmed().isEmpty(),
              !ui->in2->toPlainText().trimmed().isEmpty(),
              !ui->in3->toPlainText().trimmed().isEmpty(), language);
}

void MainWindow::on_actionCompile_triggered() {
  Log::MessageLogger::clear();
  saveFile(false, "Compiler");
  compiler->compile(editor, language);
}

void MainWindow::on_onlyRun_triggered() {
  on_runOnly_clicked();
}

void MainWindow::on_actionKill_Processes_triggered() {
  runner->killAll();
}

// ************************ ACTIONS::SETTINGS *************************
void MainWindow::on_actionChange_compile_command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Compile command",
      "Enter new Compile command (source path will be appended to end of this "
      "file)",
      QLineEdit::Normal, QString::fromStdString(setting->getCompileCommand()),
      &ok);
  if (ok && !text.isEmpty()) {
    setting->setCompileCommands(text.toStdString());
    runEditorDiagonistics();
    compiler->updateCommand(
        QString::fromStdString(setting->getCompileCommand()));
    runner->updateCompileCommand(
        QString::fromStdString(setting->getCompileCommand()));
  }
}

void MainWindow::on_actionChange_run_command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Run command arguments",
      "Enter new run command arguments (filename.out will be appended to the "
      "end of "
      "command)",
      QLineEdit::Normal, QString::fromStdString(setting->getRunCommand()), &ok);
  if (ok && !text.isEmpty()) {
    setting->setRunCommand(text.toStdString());
    runner->updateRunCommand(QString::fromStdString(setting->getRunCommand()));
  }
}

void MainWindow::on_actionChange_format_command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Format command",
      "Enter new format command (filename will be appended to end of this "
      "command)",
      QLineEdit::Normal, QString::fromStdString(setting->getFormatCommand()),
      &ok);
  if (ok && !text.isEmpty()) {
    setting->setFormatCommand(text.toStdString());
    runEditorDiagonistics();
    formatter->updateCommand(
        QString::fromStdString(setting->getFormatCommand()));
  }
}

void MainWindow::on_actionSet_Code_Template_triggered() {
  auto filename = QFileDialog::getOpenFileName(
      this, tr("Choose template File"), "",
      "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
  if (filename.isEmpty())
    return;
  setting->setTemplatePath(filename.toStdString());
  runEditorDiagonistics();
  Log::MessageLogger::info(
      "Template",
      "Template path updated. It will be effective from Next Session");
  on_textChanged_triggered();
}

void MainWindow::on_actionRun_Command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Run Command",
      "Enter new run command (use only when using python or java "
      "language)",
      QLineEdit::Normal,
      QString::fromStdString(setting->getPrependRunCommand()), &ok);
  if (ok && !text.isEmpty()) {
    setting->setPrependRunCommand(text.toStdString());
    runner->updateRunStartCommand(
        QString::fromStdString(setting->getPrependRunCommand()));
  }
}
void MainWindow::on_actionSet_Tab_Size_triggered() {
  bool ok = false;
  int newSize = QInputDialog::getInt(
      this, "Tab Size", "Set the number of chars to include under tab",
      setting->getTabStop(), 1, 20, 1, &ok);
  if (ok) {
    setting->setTabStop(newSize);
    const int tabStop = newSize;
    QFontMetrics metric(editor->font());
    editor->setTabReplaceSize(tabStop);
    editor->setTabStopDistance(tabStop * metric.horizontalAdvance(" "));
  }
}

// ************************ SLOTS ******************************************
void MainWindow::firstExecutionFinished(QString Stdout, QString Stderr) {
  Log::MessageLogger::info("Runner[1]", "Execution for first case completed");

  ui->out1->clear();
  ui->out1->setPlainText(Stdout);
  if (!Stderr.isEmpty())
    Log::MessageLogger::error("Runner[1]:[STDERR]", Stderr.toStdString(), true);
  if (Stdout.isEmpty() || expected1->isEmpty())
    return;

  if (isVerdictPass(Stdout, *expected1))
    updateVerdict(Core::Verdict::ACCEPTED, 1);
  else
    updateVerdict(Core::Verdict::WRONG_ANSWER, 1);
}
void MainWindow::secondExecutionFinished(QString Stdout, QString Stderr) {
  Log::MessageLogger::info("Runner[2]", "Execution for second case completed");

  ui->out2->clear();
  ui->out2->setPlainText(Stdout);
  if (!Stderr.isEmpty())
    Log::MessageLogger::error("Runner[2]:[STDERR]", Stderr.toStdString(), true);
  if (Stdout.isEmpty() || expected2->isEmpty())
    return;

  if (isVerdictPass(Stdout, *expected2))
    updateVerdict(Core::Verdict::ACCEPTED, 2);
  else
    updateVerdict(Core::Verdict::WRONG_ANSWER, 2);
}

void MainWindow::thirdExecutionFinished(QString Stdout, QString Stderr) {
  Log::MessageLogger::info("Runner[3]", "Execution for third case completed");
  ui->out3->clear();
  ui->out3->setPlainText(Stdout);
  if (!Stderr.isEmpty())
    Log::MessageLogger::error("Runner[3]:[STDERR]", Stderr.toStdString(), true);
  if (Stdout.isEmpty() || expected3->isEmpty())
    return;

  if (isVerdictPass(Stdout, *expected3))
    updateVerdict(Core::Verdict::ACCEPTED, 3);
  else
    updateVerdict(Core::Verdict::WRONG_ANSWER, 3);
}

void MainWindow::onSaveTimerElapsed() {
  saveFile(false, "AutoSave");
}

void MainWindow::onCompanionRequest(Network::CompanionData data) {
  applyCompanion(data);
  Log::MessageLogger::info("Companion",
                           "Established the testcases. Start Coding");
}

// **************************** LANGUAGE ***********************************

void MainWindow::on_actionC_C_triggered(bool checked) {
  if (checked) {
    ui->actionC_C->setChecked(true);
    ui->actionPython->setChecked(false);
    ui->actionJava->setChecked(false);
    setting->setDefaultLanguage("Cpp");
    runner->removeExecutable();
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    language = "Cpp";
  }
}
void MainWindow::on_actionPython_triggered(bool checked) {
  if (checked) {
    ui->actionC_C->setChecked(false);
    ui->actionPython->setChecked(true);
    ui->actionJava->setChecked(false);
    setting->setDefaultLanguage("Python");
    runner->removeExecutable();
    editor->setHighlighter(new QPythonHighlighter);
    editor->setCompleter(new QPythonCompleter);
    language = "Python";
  }
}
void MainWindow::on_actionJava_triggered(bool checked) {
  if (checked) {
    ui->actionC_C->setChecked(false);
    ui->actionPython->setChecked(false);
    ui->actionJava->setChecked(true);
    setting->setDefaultLanguage("Java");
    runner->removeExecutable();
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    language = "Java";
  }
}

// ****************************** Context Menus **************************/

void MainWindow::on_in1_customContextMenuRequested(const QPoint& pos) {
  QMenu* stdMenu = ui->in1->createStandardContextMenu(pos);
  QAction* newAction = new QAction("Expand");

  QObject::connect(newAction, &QAction::triggered, this, [this] {
    auto ptr = new Expand(ui->in1);
    ptr->setTitle("Input 1");
    ptr->setUpdate(true);
    ptr->setReadFile(true);
    ptr->show();
  });

  stdMenu->insertAction(stdMenu->actions().first(), newAction);
  stdMenu->popup(ui->in1->viewport()->mapToGlobal(pos));
}

void MainWindow::on_in2_customContextMenuRequested(const QPoint& pos) {
  QMenu* stdMenu = ui->in2->createStandardContextMenu(pos);
  QAction* newAction = new QAction("Expand");

  QObject::connect(newAction, &QAction::triggered, this, [this] {
    auto ptr = new Expand(ui->in2);
    ptr->setTitle("Input 2");
    ptr->setUpdate(true);
    ptr->setReadFile(true);
    ptr->show();
  });

  stdMenu->insertAction(stdMenu->actions().first(), newAction);
  stdMenu->popup(ui->in2->viewport()->mapToGlobal(pos));
}

void MainWindow::on_in3_customContextMenuRequested(const QPoint& pos) {
  QMenu* stdMenu = ui->in3->createStandardContextMenu(pos);
  QAction* newAction = new QAction("Expand");

  QObject::connect(newAction, &QAction::triggered, this, [this] {
    auto ptr = new Expand(ui->in3);
    ptr->setTitle("Input 3");
    ptr->setUpdate(true);
    ptr->setReadFile(true);
    ptr->show();
  });

  stdMenu->insertAction(stdMenu->actions().first(), newAction);
  stdMenu->popup(ui->in3->viewport()->mapToGlobal(pos));
}

void MainWindow::on_compiler_edit_customContextMenuRequested(
    const QPoint& pos) {
  QMenu* stdMenu = ui->compiler_edit->createStandardContextMenu(pos);
  QAction* newAction = new QAction("Expand");

  QObject::connect(newAction, &QAction::triggered, this, [this] {
    auto ptr = new Expand(this->ui->compiler_edit);
    ptr->show();
  });

  stdMenu->insertAction(stdMenu->actions().first(), newAction);
  stdMenu->popup(ui->compiler_edit->viewport()->mapToGlobal(pos));
}

void MainWindow::on_out1_customContextMenuRequested(const QPoint& pos) {
  QMenu* stdMenu = ui->out1->createStandardContextMenu(pos);
  QAction* newAction = new QAction("Expand");

  QObject::connect(newAction, &QAction::triggered, this, [this] {
    auto ptr = new Expand(ui->out1);
    ptr->setTitle("Output 1");
    ptr->setUpdate(false);
    ptr->setReadFile(false);
    ptr->show();
  });

  stdMenu->insertAction(stdMenu->actions().first(), newAction);
  stdMenu->popup(ui->out1->viewport()->mapToGlobal(pos));
}
void MainWindow::on_out2_customContextMenuRequested(const QPoint& pos) {
  QMenu* stdMenu = ui->out2->createStandardContextMenu(pos);
  QAction* newAction = new QAction("Expand");

  QObject::connect(newAction, &QAction::triggered, this, [this] {
    auto ptr = new Expand(ui->out2);
    ptr->setTitle("Output 2");
    ptr->setUpdate(false);
    ptr->setReadFile(false);
    ptr->show();
  });

  stdMenu->insertAction(stdMenu->actions().first(), newAction);
  stdMenu->popup(ui->out2->viewport()->mapToGlobal(pos));
}

void MainWindow::on_out3_customContextMenuRequested(const QPoint& pos) {
  QMenu* stdMenu = ui->out3->createStandardContextMenu(pos);
  QAction* newAction = new QAction("Expand");

  QObject::connect(newAction, &QAction::triggered, this, [this] {
    auto ptr = new Expand(ui->out3);
    ptr->setTitle("Output 3");
    ptr->setUpdate(false);
    ptr->setReadFile(false);
    ptr->show();
  });

  stdMenu->insertAction(stdMenu->actions().first(), newAction);
  stdMenu->popup(ui->out3->viewport()->mapToGlobal(pos));
}

//********************* DIFF Showers ******************

void MainWindow::on_out1_diff_clicked() {
  auto ptr = new DiffViewer(expected1, ui->out1);
  ptr->setTitle("Diffviewer for Case 1");
  ptr->show();
}

void MainWindow::on_out2_diff_clicked() {
  auto ptr = new DiffViewer(expected2, ui->out2);
  ptr->setTitle("Diffviewer for Case 2");
  ptr->show();
}

void MainWindow::on_out3_diff_clicked() {
  auto ptr = new DiffViewer(expected3, ui->out3);
  ptr->setTitle("Diffviewer for Case 3");
  ptr->show();
}

//******************** COMPANION SETTINGS ************

void MainWindow::on_actionEnable_Companion_triggered(bool checked) {
  if (checked && server == nullptr) {
    Log::MessageLogger::info("Companion", "Starting competitive Companion");
    createAndAttachServer();
  } else if (checked && server != nullptr) {
    Log::MessageLogger::info("Companion", "Already running");
  } else {
    if (server != nullptr) {
      delete server;
      server = nullptr;
    }
  }
  setting->setCompetitiveCompanionActive(checked);
}

void MainWindow::on_actionChange_Port_triggered() {
  bool ok;
  int newPort =
      QInputDialog::getInt(this, "Change companion port",
                           "Enter the port on which Editor will listen",
                           setting->getConnectionPort(), 10001, 65530, 1, &ok);
  if (ok) {
    if (server == nullptr) {
      Log::MessageLogger::info("Companion", "Updated port");
    } else {
      Log::MessageLogger::info("Companion",
                               "Changing port to " + std::to_string(newPort));
      delete server;
      server = nullptr;
      createAndAttachServer();
    }
    setting->setConnectionPort(newPort);
  }
}

//***************** HELPER FUNCTIONS *****************

bool MainWindow::isVerdictPass(QString output, QString expected) {
  output = output.remove('\r');
  expected = expected.remove('\r');
  auto a_lines = output.split('\n');
  auto b_lines = expected.split('\n');
  for (int i = 0; i < a_lines.size() || i < b_lines.size(); ++i) {
    if (i >= a_lines.size()) {
      if (b_lines[i].trimmed().isEmpty())
        continue;
      else
        return false;
    }
    if (i >= b_lines.size()) {
      if (a_lines[i].trimmed().isEmpty())
        continue;
      else
        return false;
    }
    auto a_words = a_lines[i].split(' ');
    auto b_words = b_lines[i].split(' ');
    for (int j = 0; j < a_words.size() || j < b_words.size(); ++j) {
      if (j >= a_words.size()) {
        if (b_words[j].trimmed().isEmpty())
          continue;
        else
          return false;
      }
      if (j >= b_words.size()) {
        if (a_words[j].trimmed().isEmpty())
          continue;
        else
          return false;
      }
      if (a_words[j] != b_words[j])
        return false;
    }
  }
  return true;
}

bool MainWindow::saveFile(bool force, std::string head) {
  if (openFile == nullptr) {
    if (force) {
      auto filename = QFileDialog::getSaveFileName(
          this, tr("Save File"), "",
          "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
      if (filename.isEmpty())
        return false;

      openFile = new QFile(filename);
      openFile->open(QIODevice::ReadWrite | QFile::Text);
      if (openFile->isOpen()) {
        if (openFile->write(editor->toPlainText().toStdString().c_str()) != -1)
          Log::MessageLogger::info(
              head, "Saved file : " + openFile->fileName().toStdString());
        else
          Log::MessageLogger::warn(head, "File was not saved successfully");
        this->window()->setWindowTitle("CP Editor : " + openFile->fileName());
        openFile->flush();
      } else {
        Log::MessageLogger::error(
            head, "Cannot Save file. Do I have write permission?");
      }
    }
    else
      return false;
  } else {
    openFile->resize(0);
    openFile->write(editor->toPlainText().toStdString().c_str());
    openFile->flush();
    Log::MessageLogger::info(
        head, "Saved with file name " + openFile->fileName().toStdString());
  }
  on_textChanged_triggered();
  return true;
}

bool MainWindow::isTextChanged() {
  if (openFile == nullptr) {
    if (setting->getTemplatePath().size() != 0 &&
        QFile::exists(QString::fromStdString(setting->getTemplatePath()))) {
        QFile f(QString::fromStdString(setting->getTemplatePath()));
        f.open(QIODevice::ReadOnly | QFile::Text);
        return editor->toPlainText() != f.readAll();
    }
    return !editor->toPlainText().isEmpty();
  }
  if (openFile->isOpen()) {
    openFile->seek(0);
    return openFile->readAll() != editor->toPlainText();
  }
  return true;
}

bool MainWindow::closeChangedConfirm() {
  saveSettings();
  bool isChanged = isTextChanged();
  bool confirmed = !isChanged;
  if (!confirmed) {
    auto res = QMessageBox::warning(this, "Save?",
                                    "The file has been modified.\nDo you want to save your changes?",
                                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                    QMessageBox::Cancel);
    if (res == QMessageBox::Save)
      confirmed = saveFile(true, "Save");
    else if (res == QMessageBox::Discard)
      confirmed = true;
  }
  return confirmed;
}