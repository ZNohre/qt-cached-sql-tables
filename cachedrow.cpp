#include "cachedrow.h"

#include <QSqlField>

CachedRow::CachedRow(Op o, const QSqlRecord &r)
    : m_op(None)
    , m_db_values(r)
    , m_insert(o == Insert)
{
    // Initialize m_rec from db_values
    m_rec = m_db_values;

    // Ensure auto-increment fields are never marked generated
    for (int i = 0; i < m_rec.count(); ++i) {
        if (m_rec.field(i).isAutoValue()) {
            m_rec.setGenerated(i, false);
        }
    }

    setOp(o);
}

void CachedRow::setOp(Op o)
{
    //Handle clean data
    if (o == None) {
        m_submitted = true;
        m_op = None;
        m_rec = m_db_values;   // ensure rec is populated
        setGenerated(m_rec, false);
        return;
    }

    if (o == m_op)
        return;

    //Handle other operations
    m_submitted = (o != Insert && o != Delete);
    m_op = o;
    m_rec = m_db_values;
    setGenerated(m_rec, m_op == Delete);
}

const QSqlRecord &CachedRow::rec() const {
    return m_rec;
}

QSqlRecord &CachedRow::recRef() {
    return m_rec;
}

QVariant CachedRow::value(int column) const
{
    return m_rec.value(column);
}

void CachedRow::setValue(int c, const QVariant &v)
{
    m_submitted = false;
    m_rec.setValue(c, v);

    //Prevent generated flags being set on auto value columns
    if (!m_rec.field(c).isAutoValue()) {
        m_rec.setGenerated(c, true);
    }

    if (m_op == None) {
        m_op = Update;   // mark row dirty
    }
    // Insert stays Insert, Update stays Update, Delete ignored
}

bool CachedRow::submitted() const {
    return m_submitted;
}

void CachedRow::setSubmitted()
{
    m_submitted = true;
    setGenerated(m_rec, false);

    if (m_op == Delete) {
        m_rec.clearValues();
    }
    else {
        m_op = None;
        m_db_values = m_rec;
    }
}

void CachedRow::refresh(bool exists, const QSqlRecord &newvals)
{
    m_submitted = true;
    if (exists) {
        m_op = None;
        m_db_values = newvals;
        m_rec = newvals;
        setGenerated(m_rec, false);
    } else {
        m_op = Delete;
        m_rec.clear();
        m_db_values.clear();
    }
}

bool CachedRow::insert() const {
    return m_insert;
}

void CachedRow::revert()
{
    if (m_submitted)
        return; //There are no unsubmitted changes

    if (m_op == Delete)
        m_op = None;

    m_rec = m_db_values;
    setGenerated(m_rec, false);
    m_submitted = true;
}

QSqlRecord CachedRow::primaryValues(const QSqlRecord &pi) const
{
    if (m_op == Insert)
        return QSqlRecord();

    return m_db_values.keyValues(pi);
}

void CachedRow::setGenerated(QSqlRecord &r, bool g)
{
    for (int i = r.count() - 1; i >= 0; --i) {
        if (!r.field(i).isAutoValue()) {
            r.setGenerated(i, g);
        } else {
            r.setGenerated(i, false); // force auto value fields to remain false
        }
    }
}

CachedRow::Op CachedRow::op() const {
    return m_op;
}
