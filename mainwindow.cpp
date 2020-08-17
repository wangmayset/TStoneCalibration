#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFixedSize(this->width(), this->height());
    ui->addImage->setFlat(true);
    ui->calibrate->setFlat(true);
    ui->export_1->setFlat(true);
    ui->delete_1->setFlat(true);
    ui->undistort->setFlat(true);
    ui->Camera->setFlat(true);
    QMenu *menu = new QMenu();
    QAction *open_camera = menu->addAction("打开相机");
    QFont font = menu->font();
    font.setPixelSize(12);
    menu->setFont(font);
    connect(open_camera, SIGNAL(triggered()), this,SLOT(OpenCamera()));
    ui->Camera->setMenu(menu);
    ui->listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection); // 按ctrl多选
    connect(ui->addImage, SIGNAL(clicked()), this, SLOT(addImage()));
    connect(ui->delete_1, SIGNAL(clicked()), this, SLOT(deleteImage()));
    chessboard_size = 0;
    connect(ui->calibrate, SIGNAL(clicked()), this, SLOT(calibrate()));
    connect(ui->fisheye, SIGNAL(stateChanged(int)), this, SLOT(fisheyeModeSwitch(int)));
    connect(ui->export_1, SIGNAL(clicked()), this, SLOT(exportParam()));
    connect(ui->undistort, SIGNAL(clicked()), this, SLOT(distortModeSwitch()));
    connect(ui->listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(chooseImage(QListWidgetItem*, QListWidgetItem*)));
    connect(ui->double_camera, SIGNAL(toggled(bool)), this, SLOT(reset()));
    saveImage = ui->menu->addAction("导出矫正图片");
    font = saveImage->font();
    font.setPixelSize(12);
    saveImage->setFont(font);
    pNew = ui->menu->addAction("关于");
    font = pNew->font();
    font.setPixelSize(12);
    saveImage->setFont(font);
    connect(pNew, &QAction::triggered,[=](){
        QString str = "开发者：imuncle";
        str += "\n";
        str += "问题反馈: big.uncle@foxmail.com";
        QMessageBox::about(NULL, "关于", str);
        });
    connect(saveImage, SIGNAL(triggered()), this, SLOT(saveUndistort()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::OpenCamera()
{
    if(ui->single_camera->isChecked())
    {
        single_c = new single_capture();
        single_c->setWindowTitle("单个相机拍照");
        QFont font = single_c->font();
        font.setPixelSize(12);
        single_c->setFont(font);
        single_c->show();
    }
    else
    {
        double_c = new double_capture();
        double_c->setWindowTitle("两个相机拍照");
        QFont font = double_c->font();
        font.setPixelSize(12);
        double_c->setFont(font);
        double_c->show();
    }
}

void MainWindow::saveUndistort()
{
    if(calibrate_flag == false)
    {
        QMessageBox::critical(NULL, "错误", "请先标定", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    QString srcDirPath = QFileDialog::getExistingDirectory(nullptr, "选择保存路径", "./");
    if(srcDirPath.length() <= 0)
        return;
    QTextCodec *code = QTextCodec::codecForName("GB2312");//解决中文路径问题
    if(ui->double_camera->isChecked())
    {
        for(unsigned int i = 0; i < stereo_imgs.size(); i++)
        {
            struct Stereo_Img_t img = stereo_imgs[i];
            cv::Mat left_dst = img.left_img.clone();
            cv::Mat right_dst = img.right_img.clone();
            if(fisheye_flag == false)
            {
                cv::Mat mapx, mapy;
                cv::initUndistortRectifyMap(cameraMatrix, distCoefficients, R1, P1, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.left_img, left_dst, mapx, mapy, cv::INTER_LINEAR);
                cv::initUndistortRectifyMap(cameraMatrix2, distCoefficients2, R2, P2, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.right_img, right_dst, mapx, mapy, cv::INTER_LINEAR);
            }
            else
            {
                cv::Mat mapx, mapy;
                cv::fisheye::initUndistortRectifyMap(K, D, R1, P1, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.left_img, left_dst, mapx, mapy, cv::INTER_LINEAR);
                cv::fisheye::initUndistortRectifyMap(K2, D2, R2, P2, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.right_img, right_dst, mapx, mapy, cv::INTER_LINEAR);
            }
            for(int i = 1; i < 10; i++)
            {
                cv::line(left_dst, cv::Point(0, left_dst.rows*i/10), cv::Point(left_dst.cols-1, left_dst.rows*i/10), cv::Scalar(0,0,255), 2);
                cv::line(right_dst, cv::Point(0, right_dst.rows*i/10), cv::Point(right_dst.cols-1, right_dst.rows*i/10), cv::Scalar(0,0,255), 2);
            }
            cv::Mat combian_img = cv::Mat::zeros(cv::Size(img.left_img.cols*2, img.left_img.rows), CV_8UC3);
            cv::Mat new_roi = combian_img(cv::Rect(0, 0, img.left_img.cols, img.left_img.rows));
            left_dst.convertTo(new_roi, new_roi.type());
            new_roi = combian_img(cv::Rect(img.left_img.cols, 0, img.left_img.cols, img.left_img.rows));
            right_dst.convertTo(new_roi, new_roi.type());
            QString save_name = srcDirPath + "/" + img.left_file_name;
            std::string str = code->fromUnicode(save_name).data();
            cv::imwrite(str, combian_img);
        }
    }
    else
    {
        for(unsigned int i = 0; i < imgs.size(); i++)
        {
            cv::Mat dst;
            if(fisheye_flag == false)
                cv::undistort(imgs[i].img, dst, cameraMatrix, distCoefficients);
            else
                cv::fisheye::undistortImage(imgs[i].img, dst, K, D, K, imgs[i].img.size());
            QString save_name = srcDirPath + "/" + imgs[i].file_name;
            std::string str = code->fromUnicode(save_name).data();
            cv::imwrite(str, dst);
        }
    }
}

// 切换模式时删除所有图片缓存
void MainWindow::reset()
{
    chessboard_size = 0;
    calibrate_flag = false;
    distort_flag = false;
    disconnect(ui->listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(chooseImage(QListWidgetItem*, QListWidgetItem*)));
    ui->listWidget->clear();
    connect(ui->listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(chooseImage(QListWidgetItem*, QListWidgetItem*)));
    if(imgs.size() > 0)
        imgs.clear();
    if(stereo_imgs.size() > 0)
        stereo_imgs.clear();
}

// 显示选择的图像
void MainWindow::chooseImage(QListWidgetItem* item, QListWidgetItem*)
{
    int id = ui->listWidget->row(item);
    if(ui->double_camera->isChecked())
    {
        struct Stereo_Img_t img = stereo_imgs[id];
        // 立体矫正不画点
        cv::Mat left_dst = img.left_img.clone();
        cv::Mat right_dst = img.right_img.clone();
        if(distort_flag == true && calibrate_flag == true)
        {
            if(fisheye_flag == false)
            {
                cv::Mat mapx, mapy;
                cv::initUndistortRectifyMap(cameraMatrix, distCoefficients, R1, P1, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.left_img, left_dst, mapx, mapy, cv::INTER_LINEAR);
                cv::initUndistortRectifyMap(cameraMatrix2, distCoefficients2, R2, P2, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.right_img, right_dst, mapx, mapy, cv::INTER_LINEAR);
            }
            else
            {
                cv::Mat mapx, mapy;
                cv::fisheye::initUndistortRectifyMap(K, D, R1, P1, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.left_img, left_dst, mapx, mapy, cv::INTER_LINEAR);
                cv::fisheye::initUndistortRectifyMap(K2, D2, R2, P2, img_size, CV_16SC2, mapx, mapy);
                cv::remap(img.right_img, right_dst, mapx, mapy, cv::INTER_LINEAR);
            }
            for(int i = 1; i < 10; i++)
            {
                cv::line(left_dst, cv::Point(0, left_dst.rows*i/10), cv::Point(left_dst.cols-1, left_dst.rows*i/10), cv::Scalar(0,0,255), 2);
                cv::line(right_dst, cv::Point(0, right_dst.rows*i/10), cv::Point(right_dst.cols-1, right_dst.rows*i/10), cv::Scalar(0,0,255), 2);
            }
        }
        else
        {
            for(unsigned int i = 0; i < img.left_img_points.size(); i++)
            {
                cv::circle(left_dst, img.left_img_points[i], 10, cv::Scalar(0,0,255), 2);
            }
            for(unsigned int i = 0; i < img.right_img_points.size(); i++)
            {
                cv::circle(right_dst, img.right_img_points[i], 10, cv::Scalar(0,0,255), 2);
            }
            if(calibrate_flag == true)
            {
                if(fisheye_flag == false)
                {
                    std::vector<cv::Point2f> reproject_img_p;
                    projectPoints(img.world_points, img.left_rvec, img.left_tvec, cameraMatrix, distCoefficients, reproject_img_p);
                    for(unsigned int i = 0; i < reproject_img_p.size(); i++)
                    {
                        cv::circle(left_dst, reproject_img_p[i], 3, cv::Scalar(255,0,0), 2);
                    }
                    projectPoints(img.world_points, img.right_rvec, img.right_tvec, cameraMatrix2, distCoefficients2, reproject_img_p);
                    for(unsigned int i = 0; i < reproject_img_p.size(); i++)
                    {
                        cv::circle(right_dst, reproject_img_p[i], 3, cv::Scalar(255,0,0), 2);
                    }
                }
                else
                {
                    std::vector<cv::Point2d> reproject_img_p;
                    std::vector<cv::Point3d> w_p_;
                    for(unsigned int i = 0; i < img.world_points.size(); i++)
                    {
                        w_p_.push_back(img.world_points[i]);
                    }
                    cv::fisheye::projectPoints(w_p_, reproject_img_p, img.left_fish_rvec, img.left_fish_tvec, K, D);
                    for(unsigned int i = 0; i < reproject_img_p.size(); i++)
                    {
                        cv::circle(left_dst, reproject_img_p[i], 3, cv::Scalar(255,0,0), 2);
                    }
                    cv::fisheye::projectPoints(w_p_, reproject_img_p, img.right_fish_rvec, img.right_fish_tvec, K2, D2);
                    for(unsigned int i = 0; i < reproject_img_p.size(); i++)
                    {
                        cv::circle(right_dst, reproject_img_p[i], 3, cv::Scalar(255,0,0), 2);
                    }
                }
            }
        }
        int height = img.left_img.rows*265/img.left_img.cols;
        cv::resize(left_dst, left_dst, cv::Size(265, height));
        cv::resize(right_dst, right_dst, cv::Size(265, height));
        cv::Mat combian_img = cv::Mat::zeros(cv::Size(530, height), CV_8UC3);
        cv::Mat new_roi = combian_img(cv::Rect(0, 0, 265, height));
        left_dst.convertTo(new_roi, new_roi.type());
        new_roi = combian_img(cv::Rect(265, 0, 265, height));
        right_dst.convertTo(new_roi, new_roi.type());
        QImage qimage = Mat2QImage(combian_img);
        ui->Image->setPixmap(QPixmap::fromImage(qimage));
    }
    else
    {
        cv::Mat img;
        std::vector<cv::Point2f> corners;
        std::vector<cv::Point3f> w_p;
        cv::Mat rvecs, tvecs;
        cv::Vec3d fish_rvecs, fish_tvecs;
        img = imgs[id].img.clone();
        corners = imgs[id].img_points;
        w_p = imgs[id].world_points;
        tvecs = imgs[id].tvec;
        rvecs = imgs[id].rvec;
        fish_rvecs = imgs[id].fish_rvec;
        fish_tvecs = imgs[id].fish_tvec;
        for(unsigned int i = 0; i < corners.size(); i++)
        {
            cv::circle(img, corners[i], 10, cv::Scalar(0,0,255), 2);
        }
        if(calibrate_flag == true)
        {
            if(fisheye_flag == false)
            {
                std::vector<cv::Point2f> reproject_img_p;
                projectPoints(w_p, rvecs, tvecs, cameraMatrix, distCoefficients, reproject_img_p);
                for(unsigned int i = 0; i < reproject_img_p.size(); i++)
                {
                    cv::circle(img, reproject_img_p[i], 3, cv::Scalar(255,0,0), 2);
                }
            }
            else
            {
                std::vector<cv::Point2d> reproject_img_p;
                std::vector<cv::Point3d> w_p_;
                for(unsigned int i = 0; i < w_p.size(); i++)
                {
                    w_p_.push_back(w_p[i]);
                }
                cv::fisheye::projectPoints(w_p_, reproject_img_p, fish_rvecs, fish_tvecs, K, D);
                for(unsigned int i = 0; i < reproject_img_p.size(); i++)
                {
                    cv::circle(img, reproject_img_p[i], 3, cv::Scalar(255,0,0), 2);
                }
            }
        }
        if(distort_flag == true && calibrate_flag == true)
        {
            cv::Mat dst;
            if(fisheye_flag == false)
                cv::undistort(img, dst, cameraMatrix, distCoefficients);
            else
                cv::fisheye::undistortImage(img, dst, K, D, K, img.size());
            img = dst;
        }
        if(img.cols > img.rows)
            cv::resize(img, img, cv::Size(530, img.rows*530/img.cols));
        else
            cv::resize(img, img, cv::Size(img.cols*495/img.rows, 495));
        QImage qimage = Mat2QImage(img);
        ui->Image->setPixmap(QPixmap::fromImage(qimage));
    }
}

// 加载双目图像
void MainWindow::receiveFromDialog(QString str)
{
    QStringList list = str.split(",");
    QString left_src = list[0];
    QString right_src = list[1];
    chessboard_size = list[2].toDouble();
    // 加载图像
    QDir dir(left_src);
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg";
    dir.setNameFilters(filters);
    QStringList imagesList = dir.entryList();
    if(imagesList.length() <= 3)
    {
        QMessageBox::critical(NULL, "错误", "至少需要四组图片", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    QProgressDialog *dialog = new QProgressDialog(tr("检测角点..."),tr("取消"),0,imagesList.length(),this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setWindowTitle("请稍候");
    dialog->setValue(0);
    QFont font = dialog->font();
    font.setPixelSize(12);
    dialog->setFont(font);
    dialog->show();
    for(int i = 0; i < imagesList.length(); i++)
    {
        QString left_file_name = left_src + "/" + imagesList[i];
        QString right_file_name = right_src + "/" + imagesList[i];
        cv::Mat right_image = cv::imread(right_file_name.toStdString());
        if(right_image.empty())
        {
            dialog->setValue(i+1);
            continue;
        }
        cv::Mat left_image = cv::imread(left_file_name.toStdString());
        if(left_image.cols != right_image.cols || left_image.rows != right_image.rows)
        {
            QMessageBox::critical(NULL, "错误", "左右相机图片尺寸不一致", QMessageBox::Yes, QMessageBox::Yes);
            delete dialog;
            return;
        }
        struct Chessboarder_t left_chess = findCorner(left_image, 2, 0.1);
        if(left_chess.chessboard.size() != 1)
        {
            dialog->setValue(i+1);
            continue;
        }
        struct Chessboarder_t right_chess = findCorner(right_image, 2, 0.1);
        if(right_chess.chessboard.size() != 1)
        {
            dialog->setValue(i+1);
            continue;
        }
        if(left_chess.chessboard[0].rows != right_chess.chessboard[0].rows ||
                left_chess.chessboard[0].cols != right_chess.chessboard[0].cols)
        {
            dialog->setValue(i+1);
            continue;
        }
        struct Stereo_Img_t img_;
        img_.left_img = left_image;
        img_.right_img = right_image;
        img_.left_file_name = imagesList[i];
        img_.right_file_name = imagesList[i];
        for (unsigned int j = 0; j < left_chess.chessboard.size(); j++)
        {
            for (int u = 0; u < left_chess.chessboard[j].rows; u++)
            {
                for (int v = 0; v < left_chess.chessboard[j].cols; v++)
                {
                    img_.left_img_points.push_back(left_chess.corners.p[left_chess.chessboard[j].at<uint16_t>(u, v)]);
                    img_.world_points.push_back(cv::Point3f(u*chessboard_size, v*chessboard_size, 0));
                    img_.right_img_points.push_back(right_chess.corners.p[right_chess.chessboard[j].at<uint16_t>(u, v)]);
                }
            }
        }
        stereo_imgs.push_back(img_);
        img_size = left_image.size();
        int img_height = left_image.rows*90/left_image.cols;
        cv::resize(left_image, left_image, cv::Size(90, img_height));
        cv::resize(right_image, right_image, cv::Size(90, img_height));
        cv::Mat combian_img = cv::Mat::zeros(cv::Size(180, img_height), CV_8UC3);
        cv::Mat new_roi = combian_img(cv::Rect(0, 0, 90, img_height));
        left_image.convertTo(new_roi, new_roi.type());
        new_roi = combian_img(cv::Rect(90, 0, 90, img_height));
        right_image.convertTo(new_roi, new_roi.type());
        QPixmap pixmap = QPixmap::fromImage(Mat2QImage(combian_img));
        QListWidgetItem* temp = new QListWidgetItem();
        temp->setSizeHint(QSize(220, img_height));
        temp->setIcon(QIcon(pixmap));
        temp->setText(imagesList[i]);
        ui->listWidget->addItem(temp);
        ui->listWidget->setIconSize(QSize(180, img_height));
        dialog->setValue(i+1);
    }
    delete dialog;
}

// 鱼眼相机切换
void MainWindow::fisheyeModeSwitch(int state)
{
    fisheye_flag = state==0 ? false : true;
    if(fisheye_flag == true)
    {
        ui->k_2->setDisabled(true);
        ui->k_3->setDisabled(true);
        ui->tangential->setDisabled(true);
    }
    else
    {
        ui->k_2->setEnabled(true);
        ui->k_3->setEnabled(true);
        ui->tangential->setEnabled(true);
    }
}

// 畸变矫正切换
void MainWindow::distortModeSwitch()
{
    static int cnt = 0;
    cnt++;
    if(cnt%2 == 1)
    {
        distort_flag = true;
    }
    else
    {
        distort_flag = false;
    }
    int id = ui->listWidget->selectionModel()->selectedIndexes()[0].row();
    chooseImage(ui->listWidget->item(id), ui->listWidget->item(id));
}

// Mat格式转QImage格式
QImage MainWindow::Mat2QImage(cv::Mat cvImg)
{
    QImage qImg;
    if(cvImg.channels()==3)                             //3 channels color image
    {
        cv::cvtColor(cvImg,cvImg,CV_BGR2RGB);
        qImg =QImage((const unsigned char*)(cvImg.data),
                    cvImg.cols, cvImg.rows,
                    cvImg.cols*cvImg.channels(),
                    QImage::Format_RGB888);
    }
    else if(cvImg.channels()==1)                    //grayscale image
    {
        qImg =QImage((const unsigned char*)(cvImg.data),
                    cvImg.cols,cvImg.rows,
                    cvImg.cols*cvImg.channels(),
                    QImage::Format_Indexed8);
    }
    else
    {
        qImg =QImage((const unsigned char*)(cvImg.data),
                    cvImg.cols,cvImg.rows,
                    cvImg.cols*cvImg.channels(),
                    QImage::Format_RGB888);
    }
    return qImg;
}

// 单目相机图片加载
void MainWindow::addImage()
{
    if(ui->double_camera->isChecked())
    {
        d = new Dialog();
        d->setWindowTitle("选择图片文件夹");
        QFont font = d->font();
        font.setPixelSize(12);
        d->setFont(font);
        d->show();
        connect(d, SIGNAL(SendSignal(QString)), this, SLOT(receiveFromDialog(QString)));
        return;
    }
    if(chessboard_size == 0)
    {
        bool ok;
        chessboard_size = QInputDialog::getDouble(this,tr("角点间距"),tr("请输入角点间距(mm)"),20,0,1000,2,&ok);
        if(!ok)
        {
            chessboard_size = 0;
            return;
        }
    }
    QStringList path_list = QFileDialog::getOpenFileNames(this, tr("选择图片"), tr("./"), tr("图片文件(*.jpg *.png *.pgm);;所有文件(*.*);"));
    QProgressDialog *dialog = new QProgressDialog(tr("检测角点..."),tr("取消"),0,path_list.size(),this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setMinimumDuration(0);
    dialog->setWindowTitle("请稍候");
    dialog->setValue(0);
    QFont font = dialog->font();
    font.setPixelSize(12);
    dialog->setFont(font);
    dialog->show();
    for(int i = 0; i < path_list.size(); i++)
    {
        QFileInfo file = QFileInfo(path_list[i]);
        QString file_name = file.fileName();
        cv::Mat img = cv::imread(path_list[i].toStdString());
        struct Chessboarder_t chess = findCorner(img, 2, 0.1);
        if(chess.chessboard.size() == 0)
        {
            dialog->setValue(i+1);
            continue;
        }
        struct Img_t img_;
        img_.img = img;
        img_.file_name = file_name;
        std::vector<cv::Point2f> img_p;
        std::vector<cv::Point3f> world_p;
        for (unsigned int j = 0; j < chess.chessboard.size(); j++)
        {
            for (int u = 0; u < chess.chessboard[j].rows; u++)
            {
                for (int v = 0; v < chess.chessboard[j].cols; v++)
                {
                    img_p.push_back(chess.corners.p[chess.chessboard[j].at<uint16_t>(u, v)]);
                    world_p.push_back(cv::Point3f(u*chessboard_size, v*chessboard_size, 0));
                }
            }
        }
        img_.img_points = img_p;
        img_.world_points = world_p;
        imgs.push_back(img_);
        img_size = img.size();
        int img_height = img.rows*124/img.cols;
        cv::resize(img, img, cv::Size(124, img_height));
        QPixmap pixmap = QPixmap::fromImage(Mat2QImage(img));
        QListWidgetItem* temp = new QListWidgetItem();
        temp->setSizeHint(QSize(200, img_height));
        temp->setIcon(QIcon(pixmap));
        temp->setText(file_name);
        ui->listWidget->addItem(temp);
        ui->listWidget->setIconSize(QSize(124, img_height));
        dialog->setValue(i+1);
    }
    delete dialog;
}

// 删除选中的图片
void MainWindow::deleteImage()
{
    std::vector<int> delete_idx;
    foreach(QModelIndex index,ui->listWidget->selectionModel()->selectedIndexes()){
        delete_idx.push_back(index.row());
    }
    if(delete_idx.size() == 0)
        return;
    std::sort(delete_idx.begin(), delete_idx.end());
    disconnect(ui->listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(chooseImage(QListWidgetItem*, QListWidgetItem*)));
    for(int i = delete_idx.size()-1; i >= 0; i--)
    {
        ui->listWidget->takeItem(delete_idx[i]);
        if(ui->double_camera->isChecked())
            stereo_imgs.erase(stereo_imgs.begin()+delete_idx[i]);
        else
            imgs.erase(imgs.begin()+delete_idx[i]);
    }
    connect(ui->listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(chooseImage(QListWidgetItem*, QListWidgetItem*)));
}

// 相机标定
void MainWindow::calibrate()
{
    if(!ui->double_camera->isChecked())
    {
        if(imgs.size() <= 3)
        {
            QMessageBox::critical(NULL, "错误", "至少需要四张图片", QMessageBox::Yes, QMessageBox::Yes);
            return;
        }
        std::vector<std::vector<cv::Point2f>> img_points;
        std::vector<std::vector<cv::Point3f>> world_points;
        std::vector<cv::Mat> tvecsMat;
        std::vector<cv::Mat> rvecsMat;
        std::vector<std::vector<cv::Point2d> > imagePoints;
        std::vector<std::vector<cv::Point3d> > objectPoints;
        std::vector<cv::Vec3d> rvecs;
        std::vector<cv::Vec3d> tvecs;
        for (unsigned int j = 0; j < imgs.size(); j++)
        {
            img_points.push_back(imgs[j].img_points);
            world_points.push_back(imgs[j].world_points);
            std::vector<cv::Point2d> img_p;
            std::vector<cv::Point3d> obj_p;
            for(unsigned int i = 0; i < imgs[j].img_points.size(); i++)
            {
                img_p.push_back(imgs[j].img_points[i]);
                obj_p.push_back(imgs[j].world_points[i]);
            }
            imagePoints.push_back(img_p);
            objectPoints.push_back(obj_p);
        }
        cameraMatrix = cv::Mat::zeros(cv::Size(3, 3), CV_32F);
        if(fisheye_flag == false)
            distCoefficients = cv::Mat::zeros(cv::Size(5, 1), CV_32F);

        if(fisheye_flag == false)
        {
            int flag = 0;
            if(!ui->tangential->isChecked())
                flag |= CV_CALIB_ZERO_TANGENT_DIST;
            if(!ui->k_3->isChecked())
                flag |= CV_CALIB_FIX_K3;
            cv::calibrateCamera(world_points, img_points, img_size, cameraMatrix, distCoefficients, rvecsMat, tvecsMat, flag);
        }
        else
        {
            int flag = 0;
            flag |= cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;
            flag |= cv::fisheye::CALIB_FIX_SKEW;
            try {
                cv::fisheye::calibrate(objectPoints, imagePoints, img_size, K, D, rvecs, tvecs, flag);
            } catch (cv::Exception& e) {
                QMessageBox::critical(NULL, "错误", "请采集更多方位的图片或者检查角点识别！", QMessageBox::Yes, QMessageBox::Yes);
                imgs.clear();
                ui->listWidget->clear();
                return;
            }
        }
        for(unsigned int i = 0; i < imgs.size(); i++)
        {
            if(fisheye_flag == false)
            {
                imgs[i].tvec = tvecsMat[i];
                imgs[i].rvec = rvecsMat[i];
            }
            else
            {
                imgs[i].fish_tvec = tvecs[i];
                imgs[i].fish_rvec = rvecs[i];
            }
        }
        // 评估标定结果
        std::vector<float> error;
        for(unsigned int i = 0; i < imgs.size(); i++)
        {
            std::vector<cv::Point3f> world_p = imgs[i].world_points;
            std::vector<cv::Point2f> img_p = imgs[i].img_points, reproject_img_p;
            std::vector<cv::Point2d> fisheye_reproject_p;
            if(fisheye_flag == false)
                projectPoints(world_p, rvecsMat[i], tvecsMat[i], cameraMatrix, distCoefficients, reproject_img_p);
            else
                cv::fisheye::projectPoints(objectPoints[i], fisheye_reproject_p, rvecs[i], tvecs[i], K, D);
            float err = 0;
            for (unsigned int j = 0; j < img_p.size(); j++)
            {
                if(fisheye_flag == false)
                    err += sqrt((img_p[j].x-reproject_img_p[j].x)*(img_p[j].x-reproject_img_p[j].x)+
                                (img_p[j].y-reproject_img_p[j].y)*(img_p[j].y-reproject_img_p[j].y));
                else
                    err += sqrt((img_p[j].x-fisheye_reproject_p[j].x)*(img_p[j].x-fisheye_reproject_p[j].x)+
                                (img_p[j].y-fisheye_reproject_p[j].y)*(img_p[j].y-fisheye_reproject_p[j].y));
            }
            error.push_back(err/img_p.size());
        }
        int max_idx = max_element(error.begin(), error.end()) - error.begin();
        float max_error = error[max_idx];
        int width = 240 / imgs.size();
        cv::Mat error_plot = cv::Mat(260, 320, CV_32FC3, cv::Scalar(255,255,255));
        cv::rectangle(error_plot, cv::Rect(40, 20, 240, 200), cv::Scalar(0, 0, 0),1, cv::LINE_8,0);
        cv::putText(error_plot, "0", cv::Point(20, 220), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 0), 1, 8, 0);
        char *chCode;
        chCode = new(std::nothrow)char[20];
        sprintf(chCode, "%.2lf", max_error*200/195);
        std::string strCode(chCode);
        delete []chCode;
        cv::putText(error_plot, strCode, cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 0), 1, 8, 0);
        for(unsigned int i = 0; i < imgs.size(); i++)
        {
            int height = 195*error[i]/max_error;
            cv::rectangle(error_plot, cv::Rect(i*width+41, 220-height, width-2, height), cv::Scalar(255,0,0), -1, cv::LINE_8,0);
            cv::putText(error_plot, std::to_string(i), cv::Point(i*width+40, 240), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 0), 1, 8, 0);
        }
        cv::imshow("error", error_plot);
    }
    else
    {
        if(stereo_imgs.size() <= 3)
        {
            QMessageBox::critical(NULL, "错误", "至少需要四组图片", QMessageBox::Yes, QMessageBox::Yes);
            return;
        }
        std::vector<std::vector<cv::Point2f>> left_img_points;
        std::vector<std::vector<cv::Point2f>> right_img_points;
        std::vector<std::vector<cv::Point3f>> world_points;
        std::vector<cv::Mat> left_tvecsMat;
        std::vector<cv::Mat> left_rvecsMat;
        std::vector<cv::Mat> right_tvecsMat;
        std::vector<cv::Mat> right_rvecsMat;
        std::vector<std::vector<cv::Point2d> > left_imagePoints;
        std::vector<std::vector<cv::Point2d> > right_imagePoints;
        std::vector<std::vector<cv::Point3d> > objectPoints;
        std::vector<cv::Vec3d> left_rvecs;
        std::vector<cv::Vec3d> left_tvecs;
        std::vector<cv::Vec3d> right_rvecs;
        std::vector<cv::Vec3d> right_tvecs;
        for (unsigned int j = 0; j < stereo_imgs.size(); j++)
        {
            left_img_points.push_back(stereo_imgs[j].left_img_points);
            right_img_points.push_back(stereo_imgs[j].right_img_points);
            world_points.push_back(stereo_imgs[j].world_points);
            std::vector<cv::Point2d> img_p, img_p_;
            std::vector<cv::Point3d> obj_p;
            for(unsigned int i = 0; i < stereo_imgs[j].left_img_points.size(); i++)
            {
                img_p.push_back(stereo_imgs[j].left_img_points[i]);
                img_p_.push_back(stereo_imgs[j].right_img_points[i]);
                obj_p.push_back(stereo_imgs[j].world_points[i]);
            }
            left_imagePoints.push_back(img_p);
            right_imagePoints.push_back(img_p_);
            objectPoints.push_back(obj_p);
        }
        cameraMatrix = cv::Mat::zeros(cv::Size(3, 3), CV_32F);
        cameraMatrix2 = cv::Mat::zeros(cv::Size(3, 3), CV_32F);
        if(fisheye_flag == false)
        {
            distCoefficients = cv::Mat::zeros(cv::Size(5, 1), CV_32F);
            distCoefficients2 = cv::Mat::zeros(cv::Size(5, 1), CV_32F);
        }
        // 开始标定
        if(fisheye_flag == false)
        {
            int flag = 0;
            if(!ui->tangential->isChecked())
                flag |= CV_CALIB_ZERO_TANGENT_DIST;
            if(!ui->k_3->isChecked())
                flag |= CV_CALIB_FIX_K3;
            cv::calibrateCamera(world_points, left_img_points, img_size, cameraMatrix, distCoefficients, left_rvecsMat, left_tvecsMat, flag);
            cv::calibrateCamera(world_points, right_img_points, img_size, cameraMatrix2, distCoefficients2, right_rvecsMat, right_tvecsMat, flag);
        }
        else
        {
            int flag = 0;
            flag |= cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;
            flag |= cv::fisheye::CALIB_FIX_SKEW;
            cv::fisheye::calibrate(objectPoints, left_imagePoints, img_size, K, D, left_rvecs, left_tvecs, flag);
            cv::fisheye::calibrate(objectPoints, right_imagePoints, img_size, K2, D2, right_rvecs, right_tvecs, flag);
        }
        for(unsigned int i = 0; i < stereo_imgs.size(); i++)
        {
            if(fisheye_flag == false)
            {
                stereo_imgs[i].left_tvec = left_tvecsMat[i];
                stereo_imgs[i].left_rvec = left_rvecsMat[i];
                stereo_imgs[i].right_tvec = right_tvecsMat[i];
                stereo_imgs[i].right_rvec = right_rvecsMat[i];
            }
            else
            {
                stereo_imgs[i].left_fish_tvec = left_tvecs[i];
                stereo_imgs[i].left_fish_rvec = left_rvecs[i];
                stereo_imgs[i].right_fish_tvec = right_tvecs[i];
                stereo_imgs[i].right_fish_rvec = right_rvecs[i];
            }
        }
        if(fisheye_flag == false)
        {
            int flag = 0;
            if(!ui->tangential->isChecked())
                flag |= CV_CALIB_ZERO_TANGENT_DIST;
            if(!ui->k_3->isChecked())
                flag |= CV_CALIB_FIX_K3;
            flag |= CV_CALIB_USE_INTRINSIC_GUESS;
            cv::stereoCalibrate(world_points, left_img_points, right_img_points,
                                cameraMatrix, distCoefficients,
                                cameraMatrix2, distCoefficients2,
                                img_size, R, T, cv::noArray(), cv::noArray(), flag);
            cv::stereoRectify(cameraMatrix, distCoefficients,
                                  cameraMatrix2, distCoefficients2,
                                  img_size, R, T,
                                  R1, R2, P1, P2, Q,0,0, img_size);
        }
        else
        {
            int flag = 0;
            flag |= cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;
            flag |= cv::fisheye::CALIB_FIX_SKEW;
            flag |= cv::fisheye::CALIB_USE_INTRINSIC_GUESS;
            try {
                cv::fisheye::stereoCalibrate(objectPoints, left_imagePoints, right_imagePoints,
                                                    K, D,
                                                    K2, D2,
                                                    img_size, R, T, flag);
            } catch (cv::Exception& e) {
                QMessageBox::critical(NULL, "错误", "请采集更多方位的图片！", QMessageBox::Yes, QMessageBox::Yes);
                stereo_imgs.clear();
                ui->listWidget->clear();
                return;
            }

            cv::fisheye::stereoRectify(K, D,
                                  K2, D2,
                                  img_size, R, T,
                                  R1, R2, P1, P2, Q, 0, img_size);
        }
        // 评估标定结果
        std::vector<float> left_error, right_error;
        for(unsigned int i = 0; i < stereo_imgs.size(); i++)
        {
            std::vector<cv::Point3f> world_p = stereo_imgs[i].world_points;
            std::vector<cv::Point2f> left_img_p = stereo_imgs[i].left_img_points, right_img_p = stereo_imgs[i].right_img_points;
            std::vector<cv::Point2f> left_reproject_img_p, right_reproject_img_p;
            std::vector<cv::Point2d> left_fisheye_reproject_p, right_fisheye_reproject_p;
            if(fisheye_flag == false)
            {
                projectPoints(world_p, left_rvecsMat[i], left_tvecsMat[i], cameraMatrix, distCoefficients, left_reproject_img_p);
                projectPoints(world_p, right_rvecsMat[i], right_tvecsMat[i], cameraMatrix2, distCoefficients2, right_reproject_img_p);
            }
            else
            {
                cv::fisheye::projectPoints(objectPoints[i], left_fisheye_reproject_p, left_rvecs[i], left_tvecs[i], K, D);
                cv::fisheye::projectPoints(objectPoints[i], right_fisheye_reproject_p, left_rvecs[i], left_tvecs[i], K2, D2);
            }
            float left_err = 0, right_err = 0;
            for (unsigned int j = 0; j < world_p.size(); j++)
            {
                if(fisheye_flag == false)
                {
                    left_err += sqrt((left_img_p[j].x-left_reproject_img_p[j].x)*(left_img_p[j].x-left_reproject_img_p[j].x)+
                                (left_img_p[j].y-left_reproject_img_p[j].y)*(left_img_p[j].y-left_reproject_img_p[j].y));
                    right_err += sqrt((right_img_p[j].x-right_reproject_img_p[j].x)*(right_img_p[j].x-right_reproject_img_p[j].x)+
                                (right_img_p[j].y-right_reproject_img_p[j].y)*(right_img_p[j].y-right_reproject_img_p[j].y));
                }
                else
                {
                    left_err += sqrt((left_img_p[j].x-left_fisheye_reproject_p[j].x)*(left_img_p[j].x-left_fisheye_reproject_p[j].x)+
                                (left_img_p[j].y-left_fisheye_reproject_p[j].y)*(left_img_p[j].y-left_fisheye_reproject_p[j].y));
                    right_err += sqrt((right_img_p[j].x-right_fisheye_reproject_p[j].x)*(right_img_p[j].x-right_fisheye_reproject_p[j].x)+
                                (right_img_p[j].y-right_fisheye_reproject_p[j].y)*(right_img_p[j].y-right_fisheye_reproject_p[j].y));
                }
            }
            left_error.push_back(left_err/world_p.size());
            right_error.push_back(right_err/world_p.size());
        }
        int max_idx = max_element(left_error.begin(), left_error.end()) - left_error.begin();
        float max_error = left_error[max_idx];
        max_idx = max_element(right_error.begin(), right_error.end()) - right_error.begin();
        max_error = std::max(max_error, right_error[max_idx]);
        int width = 480 / stereo_imgs.size();
        cv::Mat error_plot = cv::Mat(260, 560, CV_32FC3, cv::Scalar(255,255,255));
        cv::rectangle(error_plot, cv::Rect(40, 20, 480, 200), cv::Scalar(0, 0, 0),1, cv::LINE_8,0);
        cv::putText(error_plot, "0", cv::Point(20, 220), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 0), 1, 8, 0);
        char *chCode;
        chCode = new(std::nothrow)char[20];
        sprintf(chCode, "%.2lf", max_error*200/195);
        std::string strCode(chCode);
        delete []chCode;
        cv::putText(error_plot, strCode, cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 0), 1, 8, 0);
        for(unsigned int i = 0; i < stereo_imgs.size(); i++)
        {
            int height = 195*left_error[i]/max_error;
            cv::rectangle(error_plot, cv::Rect(i*width+43, 220-height, width/2-4, height), cv::Scalar(255,0,0), -1, cv::LINE_8,0);
            height = 195*right_error[i]/max_error;
            cv::rectangle(error_plot, cv::Rect(i*width+41+width/2, 220-height, width/2-4, height), cv::Scalar(0,255,0), -1, cv::LINE_8,0);
            cv::putText(error_plot, std::to_string(i), cv::Point(i*width+40+width/2-2, 240), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 0), 1, 8, 0);
        }
        cv::imshow("error", error_plot);
    }
    calibrate_flag = true;
}

// 导出标定参数
void MainWindow::exportParam()
{
    QString strSaveName = QFileDialog::getSaveFileName(this,tr("保存的文件"),tr("calibration_param.yaml"),tr("yaml files(*.yaml)"));
    cv::FileStorage fs_write(strSaveName.toStdString(), cv::FileStorage::WRITE);
    if(ui->double_camera->isChecked())
    {
        if(fisheye_flag == false)
        {
            fs_write << "left_camera_Matrix" << cameraMatrix << "left_camera_distCoeffs" << distCoefficients;
            fs_write << "right_camera_Matrix" << cameraMatrix2 << "right_camera_distCoeffs" << distCoefficients2;
            fs_write << "Rotate_Matrix" << R << "Translate_Matrix" << T;
            fs_write << "R1" << R1 << "R2" << R2 << "P1" << P1 << "P2" << P2 << "Q" << Q;
        }
        else
        {
            cv::Mat camera_matrix = cv::Mat::zeros(cv::Size(3,3), CV_64F);
            camera_matrix.at<double>(0,0) = K(0,0);
            camera_matrix.at<double>(0,2) = K(0,2);
            camera_matrix.at<double>(1,1) = K(1,1);
            camera_matrix.at<double>(1,2) = K(1,2);
            camera_matrix.at<double>(2,2) = 1;
            cv::Mat Dist = (cv::Mat_<double>(1,4) << D[0], D[1], D[2], D[3]);
            fs_write << "left_camera_Matrix" << camera_matrix << "left_camera_distCoeffs" << Dist;
            camera_matrix.at<double>(0,0) = K2(0,0);
            camera_matrix.at<double>(0,2) = K2(0,2);
            camera_matrix.at<double>(1,1) = K2(1,1);
            camera_matrix.at<double>(1,2) = K2(1,2);
            Dist = (cv::Mat_<double>(1,4) << D2[0], D2[1], D2[2], D2[3]);
            fs_write << "right_camera_Matrix" << camera_matrix << "right_camera_distCoeffs" << Dist;
            fs_write << "Rotate_Matrix" << R << "Translate_Matrix" << T;
            fs_write << "R1" << R1 << "R2" << R2 << "P1" << P1 << "P2" << P2 << "Q" << Q;
        }
    }
    else
    {
        if(fisheye_flag == false)
            fs_write << "cameraMatrix" << cameraMatrix << "distCoeffs" << distCoefficients;
        else
        {
            cv::Mat camera_matrix = cv::Mat::zeros(cv::Size(3,3), CV_64F);
            camera_matrix.at<double>(0,0) = K(0,0);
            camera_matrix.at<double>(0,2) = K(0,2);
            camera_matrix.at<double>(1,1) = K(1,1);
            camera_matrix.at<double>(1,2) = K(1,2);
            camera_matrix.at<double>(2,2) = 1;
            cv::Mat Dist = (cv::Mat_<double>(1,4) << D[0], D[1], D[2], D[3]);
            fs_write << "cameraMatrix" << camera_matrix << "disCoeffs" << Dist;
        }
    }
    fs_write.release();
}
