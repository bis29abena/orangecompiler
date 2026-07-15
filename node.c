#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

struct vector* node_vec = NULL;
struct vector* node_vector_root = NULL;

void node_set_vector(struct vector* vec, struct vector* root)
{
    node_vec = vec;
    node_vector_root = root;
}

void node_push(struct node* node)
{
    vector_push(node_vec, &node);
}

struct node* node_peek_or_null()
{
    return vector_back_ptr_or_null(node_vec);
}

struct node* node_peek()
{
    return *(struct node**)(vector_back(node_vec));
}

struct node* node_pop()
{
    struct node* last_node = vector_back_ptr(node_vec);
    struct node* last_node_root = vector_empty(node_vec) ? NULL : vector_back_ptr(node_vector_root);
    

    vector_pop(node_vec);
    if(last_node_root == last_node)
    {
        vector_pop(node_vector_root);
    }

    return last_node;
}

struct node* node_create(struct node* _node)
{
    struct node* node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    #warning TODO: "We should set a binded owner and binded function here"

    node_push(node);
    return node;
}
