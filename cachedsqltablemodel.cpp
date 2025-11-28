#include "cachedsqltablemodel.h"

#include <algorithm>

#include <QSqlDriver>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>

using CachedSql = CachedSqlTableModelSql;

CachedSqlTableModel::CachedSqlTableModel(QObject *parent, const QSqlDatabase &db)
    : QAbstractTableModel(parent)
    , m_db(db.isValid() ? db : QSqlDatabase::database())
    , m_editQuery(m_db)
    , m_filter()
    , m_autoColumn()
    , m_select()
    , m_tableName()
    , m_fetchedCount(0)
    , m_fetchBatchSize(100)
    , m_selectQuery(m_db)
    , m_queryExhausted(false)
{
    //Ensure we have a valid database to operate on, notify if not
    if (!m_db.isOpen()) {
        m_error = QSqlError("Database not open", QString(), QSqlError::ConnectionError);
        emit errorOccurred(m_error);
    }

}

int CachedSqlTableModel::rowCount(const QModelIndex &parent) const
{
    //Range safeguards
    if (parent.isValid())
        return 0;

    return m_cache.count();
}

int CachedSqlTableModel::columnCount(const QModelIndex &parent) const
{
    //Range safeguards
    if (parent.isValid())
        return 0;

    return m_record.count();
}

QVariant CachedSqlTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // Horizontal headers
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {

        //Range safeguards
        if (section < 0 || section >= m_record.count())
            return QVariant();

        return m_record.fieldName(section);
    }

    // Vertical headers
    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {

        //Range safeguards
        if (section < 0 || section >= m_cache.count())
            return QVariant();

        return section + 1;
    }

    return QVariant();
}

QVariant CachedSqlTableModel::data(const QModelIndex &index, int role) const
{
    //Range safeguards
    if (!index.isValid())
        return QVariant();

    if(index.row() < 0 || index.row() >= m_cache.count() || index.column() < 0 || index.column() >= m_record.count())
        return QVariant();

    if(role == Qt::DisplayRole || role == Qt::EditRole){
        return m_cache[index.row()].value(index.column());
    }

    return QVariant();
}

bool CachedSqlTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    //Range safeguards
    if(!index.isValid())
        return false;

    if(index.row() < 0 || index.row() >= m_cache.count() || index.column() < 0 || index.column() >= m_record.count())
        return false;

    if(role == Qt::EditRole){

        //Confirm that the data truly changed to avoid setting generated flags except when necessary
        QVariant oldValue = data(index, role);

        if(value == oldValue)
            return false;

        //Update data structure
        m_cache[index.row()].setValue(index.column(), value); //setValue() updates CachedRow op to Update
        emit dataChanged(index, index, {role});

        return true;
    }

    return false;
}

bool CachedSqlTableModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if(parent.isValid() || row < 0 || row > m_cache.count() || count <= 0)
        return false;

    beginInsertRows(QModelIndex(), row, row + count - 1);

    for(int i = 0; i < count; ++i){
        m_cache.insert(row, CachedRow(CachedRow::Insert, m_record));
    }

    endInsertRows();

    return true;
}

bool CachedSqlTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row < 0 || row + count > m_cache.count() || count <= 0)
        return false;

    // Staged deletion - database removal will not occur until after call to submitAll()
    bool changed = false;

    for (int i = row + count - 1; i >= row; --i) {
        CachedRow &cr = m_cache[i];
        switch (cr.op()) {
            case CachedRow::None:
                cr.setOp(CachedRow::Delete);
                changed = true;
                break;
            case CachedRow::Update:
                cr.setOp(CachedRow::Delete);
                changed = true;
                break;
            case CachedRow::Insert:
                // brand-new row - should be discarded immedialty
                m_cache.removeAt(i);
                changed = true;
                break;
            case CachedRow::Delete:
                // already staged
                break;
        }
    }

    //Notify view of any changes
    if (changed)
        emit layoutChanged();

    return true;
}

Qt::ItemFlags CachedSqlTableModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return QAbstractTableModel::flags(index);

    return QAbstractTableModel::flags(index) | Qt::ItemFlag::ItemIsEditable;
}

bool CachedSqlTableModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid())
        return false;

    // If the query is active, we assume more rows may be available
    // The actual exhaustion check happens in fetchMore when next() fails
    return m_selectQuery.isActive() && !m_queryExhausted;
}

void CachedSqlTableModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid())
        return;

    int count = 0;
    QVector<CachedRow> newRows;
    newRows.reserve(m_fetchBatchSize);

    while (count < m_fetchBatchSize && m_selectQuery.next()) {
        newRows.push_back(CachedRow(CachedRow::None, m_selectQuery.record()));
        ++count;
    }

    if (count == 0) {
        // No more rows available
        m_queryExhausted = true;
        return;
    }

    beginInsertRows(QModelIndex(), m_fetchedCount, m_fetchedCount + count - 1);
    m_cache += newRows;
    m_fetchedCount += count;
    endInsertRows();
}

void CachedSqlTableModel::setSelectStatement(const QString &select)
{
    m_select = select;
}

QString CachedSqlTableModel::selectStatement() const
{
    if (m_tableName.isEmpty()) {
        m_error = QSqlError("No table name given", QString(), QSqlError::StatementError);
        emit errorOccurred(m_error);
        return QString();
    }

    QString stmt;

    //If a custom statement exists use that i.e. a stored procedure or a subset selection of the table
    if(!m_select.isEmpty())
        stmt = m_select;
    else {
        //Otherwise load the full table
        QSqlRecord rec = m_db.record(m_tableName);
        stmt = m_db.driver()->sqlStatement(QSqlDriver::SelectStatement, m_tableName, rec, false); //Todo: add filter clause, sorting will be handled on locally cached data
    }

    return CachedSql::concat(stmt, CachedSql::where(m_filter));
}

void CachedSqlTableModel::setTableName(const QString &name)
{
    clear();
    m_tableName = name;
    m_primaryIndex = m_db.primaryIndex(name);
}

QString CachedSqlTableModel::tableName() const
{
    return m_tableName;
}

QSqlRecord CachedSqlTableModel::record() const
{
    return m_record;
}

void CachedSqlTableModel::setLastError(const QSqlError &error)
{
    m_error = error;
}

QSqlError CachedSqlTableModel::lastError() const
{
    return m_error;
}

bool CachedSqlTableModel::isDirty() const
{
    for (const auto &row : std::as_const(m_cache)) {
        if (!row.submitted())
            return true;
    }

    return false;
}

bool CachedSqlTableModel::isDirty(const QModelIndex &index) const
{
    if (!index.isValid() )
        return false;

    if(index.row() < 0 || index.row() >= m_cache.count() || index.column() < 0 || index.column() >= m_record.count())
        return false;

    const CachedRow &row = m_cache.at(index.row());

    if (row.submitted())
        return false;

    return row.op() == CachedRow::Insert || row.op() == CachedRow::Delete || (row.op() == CachedRow::Update && row.rec().isGenerated(index.column()));
}

bool CachedSqlTableModel::select()
{
    QString stmt = selectStatement();
    if (stmt.isEmpty())
        return false;

    // Create/exec the forward-only query for streaming
    m_selectQuery = QSqlQuery(m_db);
    m_selectQuery.setForwardOnly(true);

    if (!m_selectQuery.exec(stmt)) {
        m_error = m_selectQuery.lastError();
        emit errorOccurred(m_error);
        return false;
    }

    beginResetModel();

    // Reset in-memory state
    m_cache.clear();
    m_record = m_selectQuery.record();
    m_autoColumn.clear();
    m_fetchedCount = 0;
    m_queryExhausted = false;

    // Detect auto column
    for (int i = 0; i < m_record.count(); ++i) {
        if (m_record.field(i).isAutoValue()) {
            m_autoColumn = m_record.fieldName(i);
            break;
        }
    }

    fetchMore();

    endResetModel();
    return true;

}

bool CachedSqlTableModel::submitAll()
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError();
        emit errorOccurred(m_error);
        return false;
    }

    bool success = true;
    QVector<int> rowsToDelete; // cache indices of rows staged-delete that succeeded in DB

    for (int row = 0; row < m_cache.count(); ++row) {
        CachedRow &cr = m_cache[row];

        if (cr.op() == CachedRow::None || cr.submitted())
            continue;

        switch (cr.op()) {
        case CachedRow::Insert:
            success = insertRowInTable(cr.rec());

            if (success) {

                //Check if we have an auto generated row, if so populate the primary key value retrieved from the insertion
                if(!m_autoColumn.isEmpty()){
                    int c = cr.rec().indexOf(m_autoColumn);

                    if(c != -1 && !cr.rec().isGenerated(c)){
                        cr.setValue(c, m_editQuery.lastInsertId());
                        emit echoLastInsertId(m_editQuery.lastInsertId());
                    }
                }

                cr.setSubmitted();
            }
            break;

        case CachedRow::Update:
            success = updateRowInTable(row, cr.rec());
            if (success) cr.setSubmitted();
            break;

        case CachedRow::Delete:
            success = deleteRowFromTable(row);
            if (success) {
                cr.setSubmitted();
                rowsToDelete.push_back(row);
            }
            break;

        default:
            qWarning() << "Unhandled case in submitAll()";
            success = false;
            break;
        }

        if (!success) {
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_db.rollback();
        return false;
    }

    // Remove committed deletes from cache with proper signals
    if (!rowsToDelete.isEmpty()) {
        std::sort(rowsToDelete.begin(), rowsToDelete.end());
        int start = rowsToDelete.front();
        int prev = start;

        auto flushRange = [&](int s, int e) {
            beginRemoveRows(QModelIndex(), s, e);
            // remove from end to start within the range
            for (int i = e; i >= s; --i) {
                m_cache.removeAt(i);
            }
            endRemoveRows();
        };

        for (int i = 1; i < rowsToDelete.size(); ++i) {
            int cur = rowsToDelete[i];
            if (cur == prev + 1) {
                prev = cur;
            } else {
                flushRange(start, prev);
                start = prev = cur;
            }
        }
        flushRange(start, prev);
    }

    return true;

}

bool CachedSqlTableModel::revertAll()
{
    bool changed = false;

    for (int row = 0; row < m_cache.count(); ++row) {
        CachedRow &cr = m_cache[row];

        switch (cr.op()) {
        case CachedRow::Insert:
            // Discard new rows entirely
            m_cache.removeAt(row);
            --row; // Adjust index after removal
            changed = true;
            break;
        case CachedRow::Update:
            cr.revert(); // Reverts to None and restores m_rec = m_db_values
            changed = true;
            break;
        case CachedRow::Delete:
            cr.revert(); // Reverts to None and restores m_rec = m_db_values
            changed = true;
            break;
        case CachedRow::None:
            break; // Already clean
        }
    }

    //Notify views of any changes
    if (changed)
        emit layoutChanged();

    return true;
}

void CachedSqlTableModel::clear()
{
    m_tableName.clear();
    m_editQuery.clear();
    m_cache.clear();
    m_record.clear();
    m_primaryIndex.clear();
    m_filter.clear();
}

bool CachedSqlTableModel::updateRowInTable(int row, const QSqlRecord &values)
{
    QSqlRecord rec(values);
    emit beforeUpdate(row, rec);

    const QSqlRecord whereValues = primaryValues(row);
    const bool prepStatement = m_db.driver()->hasFeature(QSqlDriver::PreparedQueries);
    const QString stmt = m_db.driver()->sqlStatement(QSqlDriver::UpdateStatement, m_tableName, rec, prepStatement);
    const QString where = m_db.driver()->sqlStatement(QSqlDriver::WhereStatement, m_tableName, whereValues, prepStatement);

    if (stmt.isEmpty() || where.isEmpty() || row < 0 || row >= rowCount()) {
        m_error = QSqlError("No Fields to update", QString(), QSqlError::StatementError);
        emit errorOccurred(m_error);
        return false;
    }

    return exec(CachedSql::concat(stmt, where), prepStatement, rec, whereValues);
}

bool CachedSqlTableModel::insertRowInTable(const QSqlRecord &values)
{
    QSqlRecord rec = values;

    emit beforeInsert(rec);

    const bool prepStatement = m_db.driver()->hasFeature(QSqlDriver::PreparedQueries);
    const QString stmt = m_db.driver()->sqlStatement(QSqlDriver::InsertStatement, m_tableName, rec, prepStatement);

    if (stmt.isEmpty()) {
        m_error = QSqlError("No Fields to update", QString(), QSqlError::StatementError);
        emit errorOccurred(m_error);
        return false;
    }

    return exec(stmt, prepStatement, rec, QSqlRecord() /* no where values */);
}

bool CachedSqlTableModel::deleteRowFromTable(int row)
{
    emit beforeDelete(row);

    const QSqlRecord whereValues = primaryValues(row);
    const bool prepStatement = m_db.driver()->hasFeature(QSqlDriver::PreparedQueries);
    const QString stmt = m_db.driver()->sqlStatement(QSqlDriver::DeleteStatement, m_tableName, QSqlRecord(), prepStatement);
    const QString where = m_db.driver()->sqlStatement(QSqlDriver::WhereStatement, m_tableName, whereValues, prepStatement);

    if (stmt.isEmpty() || where.isEmpty()) {
        m_error = QSqlError("Unable to delete row", QString(), QSqlError::StatementError);
        emit errorOccurred(m_error);
        return false;
    }

    return exec(CachedSql::concat(stmt, where), prepStatement, QSqlRecord() /* no new values */, whereValues);
}

QSqlRecord CachedSqlTableModel::primaryValues(int row) const
{
    const QSqlRecord &pIndex = m_primaryIndex.isEmpty() ? m_record : m_primaryIndex;

    if (row < 0 || row >= m_cache.count())
        return QSqlRecord();

    const CachedRow &cr = m_cache.at(row);

    // For Insert rows, there are no DB values yet
    if (cr.op() == CachedRow::Insert)
        return QSqlRecord();

    // For None, Update, or Delete rows, return DB baseline keys
    return cr.primaryValues(pIndex);

}

bool CachedSqlTableModel::exec(const QString &stmt, bool prepStatement, const QSqlRecord &rec, const QSqlRecord &whereValues)
{
    if (stmt.isEmpty()) {
        m_error = QSqlError("Empty SQL statement", QString(), QSqlError::StatementError);
        emit errorOccurred(m_error);
        return false;
    }

    if (prepStatement) {

        // Always clear before preparing to avoid stale binds
        m_editQuery.clear();

        if (!m_editQuery.prepare(stmt)) {
            m_error = m_editQuery.lastError();
            emit errorOccurred(m_error);
            return false;
        }

        // Bind generated fields
        for (int i = 0; i < rec.count(); ++i) {
            if (rec.isGenerated(i))
                m_editQuery.addBindValue(rec.value(i));
        }
        for (int i = 0; i < whereValues.count(); ++i) {
            if (whereValues.isGenerated(i) && !whereValues.isNull(i))
                m_editQuery.addBindValue(whereValues.value(i));
        }

        if (!m_editQuery.exec()) {
            m_error = m_editQuery.lastError();
            emit errorOccurred(m_error);
            return false;
        }
    } else {
        if (!m_editQuery.exec(stmt)) {
            m_error = m_editQuery.lastError();
            emit errorOccurred(m_error);
            return false;
        }
    }

    return true;
}

QString CachedSqlTableModel::filter() const
{
    return m_filter;
}

void CachedSqlTableModel::setFilter(const QString &filter)
{
    m_filter = filter;
}

void CachedSqlTableModel::setFetchBatchSize(int size)
{
    if (size > 0)
        m_fetchBatchSize = size;
}

int CachedSqlTableModel::fetchBatchSize() const
{
    return m_fetchBatchSize;
}

void CachedSqlTableModel::sort(int column, Qt::SortOrder order)
{

    if (m_cache.isEmpty())
        return;

    if (column < 0 || column >= m_record.count())
        return;

    emit layoutAboutToBeChanged();

    std::sort(m_cache.begin(), m_cache.end(),
              [column, order](const CachedRow &a, const CachedRow &b)
              {
                  const QVariant va = a.value(column);
                  const QVariant vb = b.value(column);

                  // Handle nulls safely
                  if (va.isNull() && vb.isNull()) return false;
                  if (va.isNull()) return (order == Qt::AscendingOrder);
                  if (vb.isNull()) return (order == Qt::DescendingOrder);

                  // Modern, type-safe comparator
                  QPartialOrdering cmp = QVariant::compare(va, vb);
                  bool less = (cmp < 0);

                  return (order == Qt::AscendingOrder) ? less : !less;
              });

    emit layoutChanged();
}
