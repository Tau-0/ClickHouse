#pragma once


#include <Storages/System/IStorageSystemOneBlock.h>


namespace DB
{

class Context;

/// system.async_loader table. Takes data from context.getAsyncLoader()
class StorageSystemAsyncLoader final : public IStorageSystemOneBlock<StorageSystemAsyncLoader>
{
public:
    std::string getName() const override { return "SystemAsyncLoader"; }

    static NamesAndTypesList getNamesAndTypes();

protected:
    using IStorageSystemOneBlock::IStorageSystemOneBlock;

    void fillData(MutableColumns & res_columns, ContextPtr context, const SelectQueryInfo & query_info) const override;
};

}
