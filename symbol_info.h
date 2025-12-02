#include<bits/stdc++.h>
using namespace std;

class symbol_info
{
private:
    string name;
    string type;
    
    // Additional attributes for different symbol types
    string symbol_type;      
    string data_type;        
    int array_size;          
    string return_type;      
    vector<pair<string, string>> parameters; 

public:
    symbol_info(string name, string type)
    {
        this->name = name;
        this->type = type;
        this->symbol_type = "";
        this->data_type = "";
        this->array_size = -1;
        this->return_type = "";
    }
    
    string get_name()
    {
        return name;
    }
    
    string get_type()
    {
        return type;
    }
    
    void set_name(string name)
    {
        this->name = name;
    }
    
    void set_type(string type)
    {
        this->type = type;
    }
    
    // Getters and setters for additional attributes
    string get_symbol_type()
    {
        return symbol_type;
    }
    
    void set_symbol_type(string symbol_type)
    {
        this->symbol_type = symbol_type;
    }
    
    string get_data_type()
    {
        return data_type;
    }
    
    void set_data_type(string data_type)
    {
        this->data_type = data_type;
    }
    
    int get_array_size()
    {
        return array_size;
    }
    
    void set_array_size(int size)
    {
        this->array_size = size;
    }
    
    string get_return_type()
    {
        return return_type;
    }
    
    void set_return_type(string return_type)
    {
        this->return_type = return_type;
    }
    
    vector<pair<string, string>>& get_parameters()
    {
        return parameters;
    }
    
    void add_parameter(string type, string name)
    {
        parameters.push_back(make_pair(type, name));
    }
    
    void set_parameters(vector<pair<string, string>> params)
    {
        this->parameters = params;
    }
    
    string getname()
    {
        return name;
    }

    ~symbol_info()
    {
        // No dynamic memory to deallocate
    }
};