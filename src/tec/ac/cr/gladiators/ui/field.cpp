#include "field.h"
#include "ui_field.h"
#include "iostream"
#include "../logic/GladiatorsList.h"
#include "../client/Client.h"
#include "../logic/pathfinding/Pathfinding.h"
#include "../logic/pathfinding/PathList.h"
#include "../logic/pathfinding/AStar.cpp"
#include <QString>

using namespace std;

Field* Field::field = nullptr;

//! A method that creates the field window
//! \param parent
//! \param stage
Field::Field(QWidget *parent, int stage) :
    QMainWindow(parent),
    ui(new Ui::Field)
{
    ui->setupUi(this);

    // Width and height of window.
    int width = 1000;
    int height = 800;
    currentStage = stage;

    // Connects designer UI with programatically added UI elements.
    scene = new QGraphicsScene(this);
    scene->setSceneRect(0,0,width, height);
    view = ui->graphicsView;
    view->setScene(scene);
    view->setFixedSize(width, height);
    soldier_scene = new QGraphicsScene(this);
    soldier_scene->setSceneRect(0,0,width, height);
    soldier_view = ui->soldierView;
    soldier_view->setScene(soldier_scene);
    soldier_view->setFixedSize(width, height);

    // Initialize background stage
    if (stage == 1) {
        ui->background->setPixmap(QPixmap("://main/fieldStage.png"));
        columns = 19;
        rows = 11;
        startingx = 125;
        startingy = 75;
    } else {
        ui->background->setPixmap(QPixmap("://main/cityStage.jpg"));
        columns = 17;
        rows = 8;
        startingx = 175;
        startingy = 165;
    }

    // Plays music.
    QMediaPlaylist *playlist = new QMediaPlaylist();
    playlist->addMedia(QUrl("qrc:/main/mainTheme.mp3"));
    playlist->setPlaybackMode(QMediaPlaylist::Loop);

    QMediaPlayer* background_music = new QMediaPlayer();
    background_music->setMedia(playlist);
    background_music->play();

    // Initialiaze Field with default grid and starts game.
    initializeField();
    game = Game::getInstance();
    game->run();

    // Initialize damage matrix and tower list.
    damageMatrix = new QList<int>;
    towerList = new QList<int>;
    for (int i = 0; i < (columns * rows); i++) {
        damageMatrix->append(0);
    }

    // Blocks placement in area limit
    if (currentStage == 1) {
        allSquares[95]->setAcceptDrops(true);
        allSquares[114]->setAcceptDrops(true);
        allSquares[133]->setAcceptDrops(true);
        allSquares[113]->setAcceptDrops(true);
        allSquares[132]->setAcceptDrops(true);
        allSquares[151]->setAcceptDrops(true);
    } else {
        allSquares[51]->setAcceptDrops(true);
        allSquares[68]->setAcceptDrops(true);
        allSquares[67]->setAcceptDrops(true);
        allSquares[84]->setAcceptDrops(true);
    }

    // Creates button hover watchers for UI Buttons.
    ButtonHoverWatcher* watcher = new ButtonHoverWatcher(this,":/main/playButtonIcon.png",":/main/playButtonIcon_pressed.png");
    ui->playButton->installEventFilter(watcher);
    ButtonHoverWatcher* watcher2 = new ButtonHoverWatcher(this,":/main/nextButtonIcon.png",":/main/nextButtonIcon_pressed.png");
    ui->nextButton->installEventFilter(watcher2);
    ButtonHoverWatcher* watcher3 = new ButtonHoverWatcher(this,":/main/resetButtonIcon.png",":/main/resetButtonIcon_pressed.png");
    ui->resetButton->installEventFilter(watcher3);
    ButtonHoverWatcher* watcher4 = new ButtonHoverWatcher(this,":/main/skipButtonIcon.png",":/main/skipButtonIcon_pressed.png");
    ui->skipButton->installEventFilter(watcher4);

    // Assigns sound effect data
    trumpet->setMedia(QUrl("qrc:/main/trumpet.mp3"));
    ding->setMedia(QUrl("qrc:/main/ding.mp3"));
    rewind->setMedia(QUrl("qrc:/main/rewind.mp3"));
    roll->setMedia(QUrl("qrc:/main/roll.mp3"));
}

/// Adds tower to pathfinding matrix
void Field::addTower(int id) {
    QList<int>* IDCoords = idToCoords(id);
    int x = IDCoords->at(0);
    int y = IDCoords->at(1);
    if (currentStage == 1) {
        fieldMatrix[x][y] = 0;
    } else {
        cityMatrix[x][y] = 0;
    }

    // Adds tower in internal tower list for rotation in UI
    towerList->append(id);
}

/// Algorithmically assign damage to square in damage matrix when adding a tower
/// @param int id position in grid
void Field::assignDamageMatrix(int id) {
    QList<int>* numbers = findCoverage(id);
    int damageIndex = allSquares[id]->damageIndex;

    // Assigns damage
    for (int i = 0; i < numbers->length(); i++) {
        int currentNumber = numbers->at(i);
        damageMatrix->insert(currentNumber, damageMatrix->at(currentNumber) + damageIndex);
    }
}

//! A method that undulls grid
/// Deopaques grid
void Field::deOpaqueGrid() {
    int dimensions = rows * columns;
    for (int i = 0; i < dimensions; i++) {
        QGraphicsRectItem* currentSquare = allSquares[i];
        if (!currentSquare->acceptDrops()) {
            currentSquare->setBrush(QBrush(QColor(0, 0, 0, 0)));
        }
    }
}

/// Deletes tower in pathfinding matrix
/// @param int id tower to delete
void Field::deleteTower(int id) {
    QList<int>* IDCoords = idToCoords(id);
    int x = IDCoords->at(0);
    int y = IDCoords->at(1);
    if (currentStage == 1) {
        fieldMatrix[x][y] = 1;
    } else {
        cityMatrix[x][y] = 1;
    }
}

/// Finds tower area coverage by ID
QList<int>* Field::findCoverage(int id) {
    QList<int>* numbers = new QList<int>();
    int up = id - columns;
    int down = id + columns;
    int left = id - 1;
    int right = id + 1;
    int upLeft = up - 1;
    int upRight = up + 1;
    int downLeft = down - 1;
    int downRight = down + 1;

    // Corrects if placed in borders
    if (left % columns == columns - 1) {
        left = -1;
        upLeft = -1;
        downLeft = -1;
    } if (right % columns == 0) {
        right = -1;
        upRight = -1;
        downRight = -1;
    } if (up < 0) {
        up = -1;
        upLeft = -1;
        upRight = -1;
    } if (down > rows * columns) {
        down = -1;
        downLeft = -1;
        downRight = -1;
    }

    // Adds to iterated list
    numbers->append(upLeft);
    numbers->append(upRight);
    numbers->append(up);
    numbers->append(left);
    numbers->append(right);
    numbers->append(downLeft);
    numbers->append(downRight);
    numbers->append(down);

    // Remove fake ones
    for (int i = 0; i < numbers->length(); i++) {
        int currentNumber = numbers->at(i);
        if (currentNumber != -1) {
            numbers->removeOne(i);
        }
    }

    return numbers;
}

Field* Field::getInstance() {
    return field;
}

/// Gets Qlist with square ID in path
QList<int>* Field::getPath() {
    PathList* pathList = PathList::getInstance();
    if (currentStage == 1) {
        pathList->createPath11x19(6, 0);
    } else {
        pathList->createPath8x17(3, 0);
    }
    QList<int>* path = pathList->toQList();
    return path;
}

QGraphicsScene* Field::getScene() {
    return this->scene;
}

QGraphicsScene* Field::getSoldierScene() {
    return this->soldier_scene;
}

/// Converts ID to x, y coordinates
/// @param int id to find coordinates
QList<int>* Field::idToCoords(int id) {
    QList<int>* result = new QList<int>();
    int x = 0;
    int y = 0;
    bool found = false;
    int internalID = 0;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (internalID == id) {
                found = true;
                break;
            }
            internalID++;
            y++;
        }
        if (found) {
            break;
        }
        x++;
        y = 0;
    }
    result->append(x);
    result->append(y);
    return result;
}

//! A method that Initializes field with default attributes.
void Field::initializeField() {
    // Grid icons.
    QRectF rect(0,0,40,40);
    double xpos = startingx;
    double ypos = startingy;
    bool isOpaque = false;

    for(int i = 0; i < rows; i++) {
        for (int i = 0; i < columns; i++) {
            CustomRectItem* item = new CustomRectItem();
            item->setRect(rect);
            item->setPen(Qt::NoPen);
            allSquares.append(item);
            item->setID(allSquares.length());
            scene->addItem(item);
            item->setPos(xpos, ypos);
            xpos += 42;
        }
        if (currentStage == 2) {
            if (isOpaque) {
                isOpaque = false;
            } else {
                isOpaque = true;
            }
        }
        ypos += 42;
        xpos = startingx;
    }

    // Draggable tower icons.
    QRectF dRect(0,0,70,70);
    xpos = 200;
    ypos = 600;
    QStringList* towerTypes = new QStringList;
    towerTypes->append("gatling");
    towerTypes->append("fire");
    towerTypes->append("electric");

    for(int i = 0; i < 3; i++) {
        DraggableRectItem* dItem = new DraggableRectItem(nullptr, towerTypes->at(i));
        scene->addItem(dItem);
        dItem->setRect(dRect);
        dItem->setPos(xpos,ypos);
        xpos += 120;
        dItem->setPen(Qt::NoPen);
        dItem->setAnchorPoint(dItem->pos());
    }
}

/// Lowers player life
void Field::lowerLife() {
    life--;
    ui->lifeLabel->setText(QString::number(life));
}

void Field::on_nextButton_clicked() {
    trumpet->play();
    resetField();
    Client::sendGladiatorsData();
    Client::retrieveGladiators();
    Pathfinding* pathfinding = Pathfinding::getInstance();
    PathList* pathList = PathList::getInstance();
    if (currentStage == 1) {
        if (pathAlgorithm) {
            pathfinding->backTrack11x19(6, 0, fieldMatrix);
            pathAlgorithm = false;
            pathList->createPath11x19(6, 0);
        } else {
            Pair src11x19 = make_pair(6, 0);
            Pair dest11x19 = make_pair(6, 18);
            aStarSearch11x19(fieldMatrix, src11x19, dest11x19);
            pathList->createPath11x19(6, 0);
        }
    } else {
        if (pathAlgorithm) {
            pathfinding->backTrack8x17(3, 0, cityMatrix);
            pathAlgorithm = false;
            pathList->createPath8x17(3, 0);
        } else {
            Pair src8x17 = make_pair(3, 0);
            Pair dest8x17 = make_pair(3, 16);
            aStarSearch8x17(cityMatrix, src8x17, dest8x17);
            pathList->createPath8x17(3, 0);
        }
    }
    game->setPath(pathList->toQList());
    game->createArmy(0);
}

void Field::on_pauseButton_clicked() {
    bool state = ui->pauseButton->isChecked();
    Game* game = Game::getInstance();
    if (state) {
        game->pause();
    } else {
        game->play();
    }
}

//! A method that is run when play button is clicked
void Field::on_playButton_clicked() {
    trumpet->play();
    // ONLINE DATA
    Client::retrieveGladiators();
    Pathfinding* pathfinding = Pathfinding::getInstance();
    pathfinding->backTrack11x19(6, 0, fieldMatrix);
    PathList* pathList = PathList::getInstance();
    pathList->createPath11x19(6, 0);
    game->setPath(pathList->toQList());
    game->createArmy(3);

    // OFFLINE TEST DATA. COMMENT IT IF RUNNING ONLINE
    /*
    QList<int>* path = new QList<int>;
    path->append(95);
    path->append(96);
    path->append(97);
    path->append(98);
    path->append(99);
    path->append(100);
    path->append(81);
    path->append(62);
    path->append(43);
    path->append(44);
    path->append(45);
    path->append(46);
    path->append(47);
    path->append(66);
    path->append(85);
    path->append(123);
    path->append(142);
    game->setPath(path);
    */
}

void Field::on_resetButton_clicked() {
    resetField();
    roll->play();
}

void Field::on_skipButton_clicked() {
    QString text = ui->genEntry->text();
    rewind->play();
}

//! A method that dulls grid
void Field::opaqueGrid() {
    bool opaque = true;
    int dimensions = rows * columns;
    QBrush clearBrush = QBrush(QColor(8, 8, 8, 30));
    QBrush opaqueBrush;
    if (currentStage == 1) {
        opaqueBrush = QBrush(QColor(80, 80, 80, 120));
    } else {
        opaqueBrush = QBrush(QColor(0, 0, 80, 120));
    }
    QBrush currentBrush;
    for (int i = 0; i < dimensions; i++) {
        QGraphicsRectItem* currentSquare = allSquares[i];
        if (opaque) {
            currentBrush = opaqueBrush;
            opaque = false;
        } else {
            currentBrush = clearBrush;
            opaque = true;
        }
        if (!currentSquare->acceptDrops()) {
            currentSquare->setBrush(currentBrush);
        }
    }
}

/// Paints designed path in UI.
/// @param QList<int> path to paint
void Field::paintPath(QList<int>* path) {
    for (int i = 0; i < path->length(); i++) {
        CustomRectItem* currentSquare = allSquares[path->at(i)];
        if (currentSquare->acceptDrops()) {
            // Ignore placed towers.
        } else {
            currentSquare->setBrush(QColor(100, 0, 0, 120));
        }
    }
}

/// Resets all field and its matrixes
void Field::resetField() {
    int dimensions = rows * columns;
    // Resets pathfinding matrix
    if (currentStage == 1) {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < columns; j++) {
                fieldMatrix[i][j] = 1;
            }
        }
    } else {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < columns; j++) {
                cityMatrix[i][j] = 1;
            }
        }
    }

    // Resets towers in UI
    for (int i = 0; i < dimensions; i++) {
        allSquares[i]->setAcceptDrops(false);
        deOpaqueGrid();
    }

    // Resets damage matrix
    damageMatrix->clear();
    for (int i = 0; i < dimensions; i++) {
        damageMatrix->append(0);
    }

    // Blocks placement in border limits
    if (currentStage == 1) {
        allSquares[95]->setAcceptDrops(true);
        allSquares[114]->setAcceptDrops(true);
        allSquares[133]->setAcceptDrops(true);
        allSquares[113]->setAcceptDrops(true);
        allSquares[132]->setAcceptDrops(true);
        allSquares[151]->setAcceptDrops(true);
    } else {
        allSquares[51]->setAcceptDrops(true);
        allSquares[68]->setAcceptDrops(true);
        allSquares[67]->setAcceptDrops(true);
        allSquares[84]->setAcceptDrops(true);
    }
}

void Field::setInstance(Field* nfield) {
    field = nfield;
}

void Field::setSoldierScene(QGraphicsScene* newScene) {
    soldier_scene = newScene;
}

Field::~Field()
{
    delete ui;
}

//! A method that shows in labels the attributes from the selected soldier
void Field::setSoldierLabels() {

    GladiatorsList* gladiatorsList = GladiatorsList::getInstance();


    int age = gladiatorsList->soldierToShow->getAge();
    QString Age = QString::number(age);
    ui->SoldierAge->setText(Age);

    int probability = gladiatorsList->soldierToShow->getSurvivalProbability();
    QString Probability = QString::number(probability);
    ui->SoldierSurvivalProbability->setText(Probability);

    int gensToSurvive = gladiatorsList->soldierToShow->getHowManyGensWillSurvive();
    QString GensToSurvive = QString::number(gensToSurvive);
    ui->SoldierHowManyGensWillSurvive->setText(GensToSurvive);

    int emotionInteligence = gladiatorsList->soldierToShow->getEmotionalInteligence();
    QString EmotionInteligence = QString::number(emotionInteligence);
    ui->SoldierEmotionalInteligence->setText(EmotionInteligence);

    int physicalCondition = gladiatorsList->soldierToShow->getPhysicalCondition();
    QString PhysicalCondition = QString::number(physicalCondition);
    ui->SoldierPhysicalCondition->setText(PhysicalCondition);

    int resistence = gladiatorsList->soldierToShow->getResistence();
    QString Resistence = QString::number(resistence);
    ui->SoldierResistence->setText(Resistence);
}

/// Gets ID of custom rect item square
/// @param CustomRectItem square
int Field::squareToID(CustomRectItem* square) {
    int id = 0;
    for (int i = 0; i < allSquares.length(); i++) {
        if (square == allSquares[i]){
            id = i;
        }
    }
    return id;
}

/// Algorithmically unassign damage to square in damage matrix when removing a tower
/// @param int id position in grid
void Field::unassignDamageMatrix(int id) {
    QList<int>* numbers = findCoverage(id);
    int damageIndex = allSquares[id]->damageIndex;

    // Unassigns damage
    for (int i = 0; i < numbers->length(); i++) {
        int currentNumber = numbers->at(i);
        damageMatrix->insert(currentNumber, damageMatrix->at(currentNumber) - damageIndex);
    }
}
