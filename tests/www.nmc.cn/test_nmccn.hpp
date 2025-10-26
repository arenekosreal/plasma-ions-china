#pragma once

#include <QTest>

class TestNmcCnIon : public QObject
{
    Q_OBJECT
private Q_SLOTS:
#ifndef ION_LEGACY
    void testFindPlaces();
    void testFetchForecast();
#endif // ION_LEGACY
};
