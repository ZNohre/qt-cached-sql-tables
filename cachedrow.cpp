#include "cachedrow.h"

#include <QSqlField>

CachedRow::CachedRow(Op o, const QSqlRecord &r)
    : m_op(None)
    , m_db_values(r)
{
    setOp(o);
}

void CachedRow::setOp(Op o)
{
    //Handle clean data
    if (o == None) {
        m_submitted = true;
        m_op = None;
        m_rec = m_db_values;   //Ensure that rec is populated from db_values to present in cached model
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

const QSqlRecord &CachedRow::rec() const
{
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
    //Flag row as having changes and assign the new value to the record
    m_submitted = false;
    m_rec.setValue(c, v);

    //Check to ensure we're not operating on an auto increment field, setting the generated flag if not
    if (!m_rec.field(c).isAutoValue()) {
        m_rec.setGenerated(c, true);
    }

    if (m_op == None) {
        m_op = Update;   //Mark row dirty
    }
    // Insert stays Insert, Update stays Update, Delete ignored
}

bool CachedRow::submitted() const {
    return m_submitted;
}

void CachedRow::setSubmitted()
{
    //Mark the row as submitted and reset generated flags
    m_submitted = true;
    setGenerated(m_rec, false);

    //If the record was flagged as a delete, remove all record values
    if (m_op == Delete) {
        m_rec.clearValues();
    } else {
        //Otherwise, return state to None and assign the db_values to the submitted rec values
        m_op = None;
        m_db_values = m_rec;
    }
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
