#ifndef CACHEDSQLTABLEMODEL_H
#define CACHEDSQLTABLEMODEL_H

#include "cachedrow.h"

#include <QAbstractTableModel>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlIndex>
#include <QSqlQuery>
#include <QSqlRecord>

typedef QVector<CachedRow> CacheVec;

class CachedSqlTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit CachedSqlTableModel(QObject *parent = nullptr, const QSqlDatabase &db = QSqlDatabase());

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool canFetchMore(const QModelIndex &parent = QModelIndex()) const override;
    void fetchMore(const QModelIndex &parent = QModelIndex()) override;

    void setSelectStatement(const QString &select);
    QString selectStatement() const;

    void setTableName(const QString &name);
    QString tableName() const;

    QSqlRecord record() const;

    void setLastError(const QSqlError &error);
    QSqlError lastError() const;

    bool isDirty() const;
    bool isDirty(const QModelIndex &index) const;

    QString filter() const;
    void setFilter(const QString &filter);

    void setFetchBatchSize(int size);
    int fetchBatchSize() const;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

public slots:
    bool select();
    bool submitAll();
    bool revertAll();
    void clear();

signals:
    void errorOccurred(const QSqlError &error) const;

    void beforeInsert(QSqlRecord &record);
    void beforeUpdate(int row, QSqlRecord &record);
    void beforeDelete(int row);

    void echoLastInsertId(const QVariant &id);

protected:
    bool updateRowInTable(int row, const QSqlRecord &values);
    bool insertRowInTable(const QSqlRecord &values);
    bool deleteRowFromTable(int row);

    QSqlRecord primaryValues(int row) const;

    bool exec(const QString &stmt, bool prepStatement, const QSqlRecord &rec, const QSqlRecord &whereValues);

protected:
    QSqlDatabase m_db;
    QSqlQuery m_editQuery;

    QSqlRecord m_record;
    QSqlIndex m_primaryIndex;

    QString m_filter;
    QString m_autoColumn;

    mutable QSqlError m_error;

    CacheVec m_cache;

    QString m_select;
    QString m_tableName;

    int m_fetchedCount;
    int m_fetchBatchSize;
    QSqlQuery m_selectQuery;
    bool m_queryExhausted;
};

// helpers for building SQL expressions
class CachedSqlTableModelSql
{
public:
    // SQL keywords
    inline const static QLatin1StringView as() { return QLatin1StringView("AS"); }
    inline const static QLatin1StringView asc() { return QLatin1StringView("ASC"); }
    inline const static QLatin1StringView comma() { return QLatin1StringView(","); }
    inline const static QLatin1StringView desc() { return QLatin1StringView("DESC"); }
    inline const static QLatin1StringView eq() { return QLatin1StringView("="); }
    // "and" is a C++ keyword
    inline const static QLatin1StringView et() { return QLatin1StringView("AND"); }
    inline const static QLatin1StringView from() { return QLatin1StringView("FROM"); }
    inline const static QLatin1StringView leftJoin() { return QLatin1StringView("LEFT JOIN"); }
    inline const static QLatin1StringView on() { return QLatin1StringView("ON"); }
    inline const static QLatin1StringView orderBy() { return QLatin1StringView("ORDER BY"); }
    inline const static QLatin1StringView parenClose() { return QLatin1StringView(")"); }
    inline const static QLatin1StringView parenOpen() { return QLatin1StringView("("); }
    inline const static QLatin1StringView select() { return QLatin1StringView("SELECT"); }
    inline const static QLatin1StringView sp() { return QLatin1StringView(" "); }
    inline const static QLatin1StringView where() { return QLatin1StringView("WHERE"); }

    // Build expressions based on key words
    inline const static QString as(const QString &a, const QString &b) { return b.isEmpty() ? a : concat(concat(a, as()), b); }
    inline const static QString asc(const QString &s) { return concat(s, asc()); }
    inline const static QString comma(const QString &a, const QString &b) { return a.isEmpty() ? b : b.isEmpty() ? a : QString(a).append(comma()).append(b); }
    inline const static QString concat(const QString &a, const QString &b) { return a.isEmpty() ? b : b.isEmpty() ? a : QString(a).append(sp()).append(b); }
    inline const static QString desc(const QString &s) { return concat(s, desc()); }
    inline const static QString eq(const QString &a, const QString &b) { return QString(a).append(eq()).append(b); }
    inline const static QString et(const QString &a, const QString &b) { return a.isEmpty() ? b : b.isEmpty() ? a : concat(concat(a, et()), b); }
    inline const static QString from(const QString &s) { return concat(from(), s); }
    inline const static QString leftJoin(const QString &s) { return concat(leftJoin(), s); }
    inline const static QString on(const QString &s) { return concat(on(), s); }
    inline const static QString orderBy(const QString &s) { return s.isEmpty() ? s : concat(orderBy(), s); }
    inline const static QString paren(const QString &s) { return s.isEmpty() ? s : parenOpen() + s + parenClose(); }
    inline const static QString select(const QString &s) { return concat(select(), s); }
    inline const static QString where(const QString &s) { return s.isEmpty() ? s : concat(where(), s); }
};

#endif // CACHEDSQLTABLEMODEL_H
