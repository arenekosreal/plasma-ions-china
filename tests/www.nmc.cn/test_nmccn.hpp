#pragma once

#include <QTest>

class TestNmcCnIon : public QObject
{
    Q_OBJECT
private slots:
#ifndef ION_LEGACY
    void testFindPlaces();
    void testFetchForecast();
#endif // ION_LEGACY
};
