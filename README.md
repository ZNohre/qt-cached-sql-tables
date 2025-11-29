# CachedSqlTableModel

A lightweight **Qt/C++ cached SQL table model** built on `QAbstractTableModel`.  
Provides efficient row caching, lazy loading, and safe commit/revert operations for database tables.

## Usage

```cpp
CachedSqlTableModel *model = new CachedSqlTableModel(nullptr, db);
model->setTableName("customers");
model->select();

QTableView *view = new QTableView;
view->setModel(model);
view->show();
