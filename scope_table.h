#include "symbol_info.h"

class scope_table
{
private:
    int bucket_count;
    int unique_id;
    scope_table *parent_scope = NULL;
    vector<list<symbol_info *>> table;

    int hash_function(string name)
    {
        // djb2 hash function
        unsigned long hash = 5381;
        for (char c : name) {
            hash = ((hash << 5) + hash) + c; // hash * 33 + c
        }
        return hash % bucket_count;
    }

public:
    scope_table();
    scope_table(int bucket_count, int unique_id, scope_table *parent_scope);
    scope_table *get_parent_scope();
    int get_unique_id();
    symbol_info *lookup_in_scope(symbol_info* symbol);
    bool insert_in_scope(symbol_info* symbol);
    bool delete_from_scope(symbol_info* symbol);
    void print_scope_table(ofstream& outlog);
    ~scope_table();

    
};

//methods of scope_table class

scope_table::scope_table()
{
    this->bucket_count = 10;
    this->unique_id = 1;
    this->parent_scope = NULL;
    table.resize(bucket_count);
}

scope_table::scope_table(int bucket_count, int unique_id, scope_table *parent_scope)
{
    this->bucket_count = bucket_count;
    this->unique_id = unique_id;
    this->parent_scope = parent_scope;
    table.resize(bucket_count);
}

scope_table *scope_table::get_parent_scope()
{
    return parent_scope;
}

int scope_table::get_unique_id()
{
    return unique_id;
}

symbol_info *scope_table::lookup_in_scope(symbol_info* symbol)
{
    if (symbol == NULL) 
        return NULL;
    
    string name = symbol->get_name();
    int index = hash_function(name);
    
    
    for (auto it = table[index].begin(); it != table[index].end(); ++it)
    {
        if ((*it)->get_name() == name)
        {
            return *it;
        }
    }
    
    return NULL;
}

bool scope_table::insert_in_scope(symbol_info* symbol)
{
    if (symbol == NULL) 
        return false;
    
    
    if (lookup_in_scope(symbol) != NULL)
    {
        return false; // Symbol already exists
    }
    
    
    int index = hash_function(symbol->get_name());
    table[index].push_back(symbol);
    
    return true;
}

bool scope_table::delete_from_scope(symbol_info* symbol)
{
    if (symbol == NULL) 
        return false;
    
    string name = symbol->get_name();
    int index = hash_function(name);
    
    
    for (auto it = table[index].begin(); it != table[index].end(); ++it)
    {
        if ((*it)->get_name() == name)
        {
            delete *it;
            table[index].erase(it);
            return true;
        }
    }
    
    return false;
}

void scope_table::print_scope_table(ofstream& outlog)
{
    outlog << "ScopeTable # " << to_string(unique_id) << endl;

    
    for (int i = 0; i < bucket_count; i++)
    {
        if (!table[i].empty())
        {
            outlog << " " << i << " --> ";
            
            bool first = true;
            for (auto symbol : table[i])
            {
                if (!first) 
                    outlog << " , ";
                first = false;
                
                outlog << "< " << symbol->get_name();
                
                
                if (symbol->get_symbol_type() == "function")
                {
                    outlog << " : Function, ReturnType: " << symbol->get_return_type();
                    
                    
                    auto params = symbol->get_parameters();
                    outlog << ", Parameters: (";
                    for (size_t j = 0; j < params.size(); j++)
                    {
                        if (j > 0) outlog << ", ";
                        outlog << params[j].first;
                        if (!params[j].second.empty())
                            outlog << " " << params[j].second;
                    }
                    outlog << ")";
                }
                else if (symbol->get_symbol_type() == "array")
                {
                    outlog << " : Array, Type: " << symbol->get_data_type();
                    outlog << ", Size: " << symbol->get_array_size();
                }
                else if (symbol->get_symbol_type() == "variable")
                {
                    outlog << " : Variable, Type: " << symbol->get_data_type();
                }
                else
                {
                    
                    outlog << " : " << symbol->get_type();
                }
                
                outlog << " >";
            }
            outlog << endl;
        }
    }
    outlog << endl;
}

scope_table::~scope_table()
{
    
    for (int i = 0; i < bucket_count; i++)
    {
        for (auto symbol : table[i])
        {
            delete symbol;
        }
        table[i].clear();
    }
}