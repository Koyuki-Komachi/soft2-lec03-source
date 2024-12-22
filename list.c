#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const int maxlen = 1000;

typedef struct node Node;

struct node {
    char *str;
    Node *next;
};

typedef struct list{
    Node *begin;
} List;

Node *push_front(List *list, const char *str) {
    Node *p = (Node*)malloc(sizeof(Node));
    char *s = (char*)malloc(strlen(str) + 1);
    strcpy(s, str);
    *p = (Node){.str = s, .next = list->begin};
    list->begin = p;
    return p;
}


Node *pop_front(List *list) {
    Node *p = list->begin;
    if (p != NULL){
        list->begin = p->next;
        p->next = NULL;
    }
    return p;
}

Node *push_back(List *list, const char *str) {
    Node *p = (Node*)malloc(sizeof(Node));
    p = list->begin;

    if (p == NULL){
        return push_front(list, str);
    }
    while (p->next != NULL){
        p = p->next;
    }
    char *s = (char*)malloc(strlen(str) + 1);
    strcpy(s, str);
    Node *q = (Node*)malloc(sizeof(Node));
    *q = (Node){.str = s, .next = NULL};
    p->next = q;
    return q;
}

// Let's try: pop_back の実装
Node *pop_back(List *list) {
    Node *p = list->begin;
    Node *q = NULL;

    if (p == NULL){
        return NULL;
    }
    while (p->next != NULL){
        q = p;
        p = p->next;
    }
    if (q == NULL){
        return pop_front(list);
    }
    if (q != NULL){
        q->next = NULL;
    }
}


Node *remove_all(Node *begin) {
    while ((begin = pop_front(begin))) 
	; // Repeat pop_front() until the list becomes empty
    return begin;
}

Node *insert(Node *p, const char *str){
    assert(p != NULL);
    Node *q = (Node*)malloc(sizeof(Node));
    char *s = (char*)malloc(strlen(str) + 1);
    strcpy(s, str);

    *q = (Node){.str = s, .next = p->next};
    p->next = q;

    return p;
}

int main() {
    Node *begin = NULL; 
    
    
    char buf[maxlen];
    while (fgets(buf, maxlen, stdin)) {
	//begin = push_front(begin, buf);
	begin = push_back(begin, buf); 
    }
    
    //begin = pop_front(begin);  
    //begin = pop_back(begin);   
    
    //begin = remove_all(begin); 
    
    // 巣鴨を駒込のあとに挿入（反時計回りに駅を並べていることに注意）
    Node *p = begin;
    while (p != NULL){
        if (strncmp(p->str, "Komagome", 8) == 0){
            insert(p, "Sugamo\n");
            break;
        }
        p = p->next;
    }

    for (const Node *p = begin; p != NULL; p = p->next) {
	printf("%s", p->str);
    }
    
    return EXIT_SUCCESS;
}
