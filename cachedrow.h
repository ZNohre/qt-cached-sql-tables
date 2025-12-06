#ifndef CACHEDROW_H
#define CACHEDROW_H

#include <QDebug>
#include <QSqlRecord>
#include <QVariant>

class CachedRow
{
public:

    enum Op {
        None,
        Insert,
        Update,
        Delete
    };

    CachedRow(Op o = None, const QSqlRecord &r = QSqlRecord());

    Op op() const;
    void setOp(Op o);

    const QSqlRecord &rec() const;
    QSqlRecord& recRef();

    QVariant value(int column) const;
    void setValue(int c, const QVariant &v);

    bool submitted() const;
    void setSubmitted();

    void revert();
    QSqlRecord primaryValues(const QSqlRecord& pi) const;

private:
    static void setGenerated(QSqlRecord& r, bool g);

    Op m_op;
    QSqlRecord m_rec;
    QSqlRecord m_db_values;
    bool m_submitted;
};

#endif // CACHEDROW_H
