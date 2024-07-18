#include "map.h"
#include "stdlib.h"

#include "stdio.h" //FIXME: remove

#define max(a, b) (a > b ? a : b)

typedef struct Node{
    struct Node *left;
    struct Node *right;
    int childDepth;
    int key;
    void *value;
}Node;

static int add(const struct Map *map, int key, void* value);
static int remove(const struct Map *map, int key);
static void* get(const struct Map *map, int key);
static int contains(const struct Map *map, int key);
static void freeMap(struct Map *map, void (*freeValue)(void *value));
static int validate(const struct Map *map);

static Node *addNode(Node *node, int key, void* value);
static Node *removeNode(Node *node, int key);
static void* getNode(Node *node, int key);
static int containsNode(Node *node, int key);
static void freeNode(Node *node, void (*freeValue)(void *value));
static int validateNode(Node *node);

static int getChildDepth(Node *node);

static Node *balanceTree(Node *root);
static Node *rotateLeft(Node *root);
static Node *rotateRight(Node *root);

Map *map_newBinaryMap(){
    Map *map = malloc(sizeof(Map));
    Node **rootPointer = malloc(sizeof(Node *));
    *rootPointer = 0;
    *map = (Map){
        .data = rootPointer,
        .add = add,
        .remove = remove,
        .get = get,
        .contains = contains,
        .free = freeMap,
        .validate = validate
    };
    return map;
}

static int add(const struct Map *map, int key, void* value){
    if(contains(map, key)){
        return 0;
    }
    Node **rootPointer = map->data;
    *rootPointer = addNode(*rootPointer, key, value);

    return 1;
}

static Node *addNode(Node *node, int key, void* value){
    if(node == 0){
        Node *newNode = malloc(sizeof(Node));
        *newNode = (Node){
            .key = key,
            .value = value,
            .childDepth = 0,
            .left = 0,
            .right = 0,
        };
        return newNode;
    }

    if(key > node->key){
        node->right = addNode(node->right, key, value);
    }
    else{
        node->left = addNode(node->left, key, value);
    }

    node->childDepth = max(getChildDepth(node->left), getChildDepth(node->right)) + 1;
    Node *newRoot = balanceTree(node);
    return newRoot;
}
static int remove(const struct Map *map, int key){
    if(!contains(map, key)){
        return 0;
    }
    Node **rootNode = map->data;
    *rootNode = removeNode(*rootNode, key);
    return 1;
}
static Node *getLeftmost(Node *root){
    while(root->left){
        root = root->left;
    }
    return root;
}
static Node *removeNode(Node *root, int key){
    if(!root){
        return 0;
    } 

    if(key > root->key){
        root->right = removeNode(root->right, key);
    }
    else if(key < root->key){
        root->left = removeNode(root->left, key);
    }
    else{
        Node *res;
        if(!root->left && !root->right){
            res = 0; 
        }
        else if(!root->right){
            res = root->left;
        }
        else if(!root->left){
            res = root->right;
        }
        else{
            res = getLeftmost(root->right);
            res->right = removeNode(root->right, res->key);
            res->left = root->left;
            res->childDepth = max(getChildDepth(res->left), getChildDepth(res->right)) + 1;
        }
        free(root);
        return res;
    }
   
    root->childDepth = max(getChildDepth(root->left), getChildDepth(root->right)) + 1;
    root = balanceTree(root);
    return root;
}
static void* get(const struct Map *map, int key){
    Node **rootNode = map->data;
    return getNode(*rootNode, key);
}
static void* getNode(Node *node, int key){
    while(node){
        if(key < node->key){
            node = node->left;
        }
        else if(key > node->key){
            node = node->right;
        }
        else{
            return node->value;
        }
    }
    return 0;
}
static int contains(const struct Map *map, int key){
    Node **rootNode = map->data;
    return containsNode(*rootNode, key);
}
static int containsNode(Node *node, int key){
    while(node){
        if(key < node->key){
            node = node->left;
        }
        else if(key > node->key){
            node = node->right;
        }
        else{
            return 1;
        }
    }
    return 0;

}
static void freeMap(struct Map *map, void (*freeValue)(void *value)){
    Node **rootNode = map->data;
    freeNode(*rootNode, freeValue);

    free(rootNode);
    free(map);
}
static void freeNode(Node *node, void (*freeValue)(void *value)){
    if(!node){
        return;
    }
    freeNode(node->left, freeValue);
    freeNode(node->right, freeValue);
    if(freeValue){
        freeValue(node->value);
    }
    free(node);
}
static int calculateChildDepth(Node *node);
static Node *balanceTree(Node *root){
    int depthLeft = getChildDepth(root->left);
    int depthRight = getChildDepth(root->right);

    if(depthRight > depthLeft + 1){
        int depthRightLeft = getChildDepth(root->right->left);
        int depthRightRight = getChildDepth(root->right->right);
        //Right left
        if(depthRightLeft > depthRightRight){
            root->right = rotateRight(root->right);
            
            root->right->right->childDepth = calculateChildDepth(root->right->right);
            root->right->childDepth = max(getChildDepth(root->right->left), getChildDepth(root->right->right)) + 1;
        }

        Node *newRoot = rotateLeft(root);
        newRoot->left->childDepth = max(getChildDepth(newRoot->left->left), getChildDepth(newRoot->left->right)) + 1;
        newRoot->childDepth = max(getChildDepth(newRoot->left), getChildDepth(newRoot->right)) + 1;
        return newRoot;
    }
    if(depthLeft > depthRight + 1){
        int depthLeftLeft = getChildDepth(root->left->left);
        int depthLeftRight = getChildDepth(root->left->right);
        //Left right
        if(depthLeftRight > depthLeftLeft){
            root->left = rotateLeft(root->left);

            root->left->left->childDepth = calculateChildDepth(root->left->left);
            root->left->childDepth = max(getChildDepth(root->left->left), getChildDepth(root->left->right));
        }

        Node *newRoot = rotateRight(root);
        newRoot->right->childDepth = max(getChildDepth(newRoot->right->left), getChildDepth(newRoot->right->right)) + 1;
        newRoot->childDepth = max(getChildDepth(newRoot->left), getChildDepth(newRoot->right)) + 1;
        return newRoot;
    }

    return root;
}
static Node *rotateLeft(Node *root){
    Node *newRoot = root->right;

    root->right = newRoot->left;
    newRoot->left = root;

    return newRoot;
}
static Node *rotateRight(Node *root){
    Node *newRoot = root->left;
    
    root->left = newRoot->right;
    newRoot->right = root;

    return newRoot;
}

static int getChildDepth(Node *node){
    if(!node){
        return -1;
    }
    return node->childDepth;
}

static int validate(const struct Map *map){
    Node **rootNode = map->data;
    return validateNode(*rootNode);
}

static int calculateChildDepth(Node *node){
    if(!node){
        return -1;
    }
    if(!node->left && !node->right){
        return 0;
    }

    int lDepth = calculateChildDepth(node->left);
    int rDepth = calculateChildDepth(node->right);

    return max(lDepth, rDepth) + 1;
}

static int isBalanced(Node *node){
    if(!node){
        return 1;
    }
    if(!isBalanced(node->left) || !isBalanced(node->right)){
        return 0;
    }
    int lDepth = calculateChildDepth(node->left);
    int rDepth = calculateChildDepth(node->right);
    int diff = lDepth - rDepth;

    return diff <= 1 && diff >= -1;
}

static int isOrdered(Node *node){
    if(!node){
        return 1;
    }
    if(node->left && node->left->key >= node->key){
        return 0;
    }
    if(node->right && node->right->key <= node->key){
        return 0;
    }
    return 1;
}

static int validateNode(Node *node){
    if(!node){
        return 1;
    }
    if(!validateNode(node->left) || !validateNode(node->right)){
        return 0;
    }

    return calculateChildDepth(node) == node->childDepth &&
           isBalanced(node) &&
           isOrdered(node);
}
