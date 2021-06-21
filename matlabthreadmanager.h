#ifndef MATLABTHREADMANAGER_H
#define MATLABTHREADMANAGER_H

#include "MatlabEngine.hpp"
#include "MatlabDataArray.hpp"
#include "matlabthread.h"
#include <queue>
#include <QtCore>
#include <QThread>

using namespace matlab::engine;

class matlabThreadManager : public QThread
{
    Q_OBJECT
public:
    matlabThreadManager(QObject *parent = 0);
    ~matlabThreadManager();
    void run();
public slots:
    void onJobStart(const size_t outA, const std::vector<matlab::data::Array> &data, const std::string funcType);
signals:
    void enableSubmitButton();
private:
    std::deque<matlabThread*> mThreads;
    std::unique_ptr<MATLABEngine> matlabPtr;
    matlab::data::ArrayFactory factory;
    size_t outA;
    std::vector<matlab::data::Array> data;
    std::string funcType;
};

#endif // MATLABTHREADMANAGER_H
