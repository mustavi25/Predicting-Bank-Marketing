#include "scope_table.h"

class symbol_table
{
private:
    scope_table *current_scope;
    int bucket_count;
    int current_scope_id;

public:
    symbol_table(int bucket_count);
    ~symbol_table();
    void enter_scope();
    void exit_scope();
    bool insert(symbol_info* symbol);
    symbol_info* lookup(symbol_info* symbol);
    void print_current_scope(ofstream& outlog);
    void print_all_scopes(ofstream& outlog);
    scope_table* get_current_scope();

    // you can add more methods if you need 
};

// Complete the methods of symbol_table class

symbol_table::symbol_table(int bucket_count)
{
    this->bucket_count = bucket_count;
    this->current_scope_id = 0;
    this->current_scope = NULL;
    
    // Enter the global scope
    enter_scope();
}

symbol_table::~symbol_table()
{
    // Delete all scope tables
    while (current_scope != NULL)
    {
        scope_table *temp = current_scope;
        current_scope = current_scope->get_parent_scope();
        delete temp;
    }
}

void symbol_table::enter_scope()
{
    // Increment scope ID
    current_scope_id++;
    
    // Create new scope table with current_scope as parent
    scope_table *new_scope = new scope_table(bucket_count, current_scope_id, current_scope);
    
    // Make the new scope the current scope
    current_scope = new_scope;
}

void symbol_table::exit_scope()
{
    if (current_scope == NULL)
        return;
    
    // Save the current scope to delete
    scope_table *temp = current_scope;
    
    // Move to parent scope
    current_scope = current_scope->get_parent_scope();
    
    // Delete the old scope
    delete temp;
}

bool symbol_table::insert(symbol_info* symbol)
{
    if (current_scope == NULL || symbol == NULL)
        return false;
    
    // Insert into current scope
    return current_scope->insert_in_scope(symbol);
}

symbol_info* symbol_table::lookup(symbol_info* symbol)
{
    if (symbol == NULL)
        return NULL;
    
    // Start searching from current scope
    scope_table *temp = current_scope;
    
    // Search through all scopes from current to global
    while (temp != NULL)
    {
        symbol_info *found = temp->lookup_in_scope(symbol);
        if (found != NULL)
        {
            return found;
        }
        // Move to parent scope
        temp = temp->get_parent_scope();
    }
    
    // Symbol not found in any scope
    return NULL;
}

void symbol_table::print_current_scope(ofstream& outlog)
{
    if (current_scope != NULL)
    {
        current_scope->print_scope_table(outlog);
    }
}

void symbol_table::print_all_scopes(ofstream& outlog)
{
    outlog << "################################" << endl << endl;
    
    scope_table *temp = current_scope;
    
    // Print all scope tables from current to global
    while (temp != NULL)
    {
        temp->print_scope_table(outlog);
        temp = temp->get_parent_scope();
    }
    
    outlog << "################################" << endl << endl;
}

scope_table* symbol_table::get_current_scope()
{
    return current_scope;
}