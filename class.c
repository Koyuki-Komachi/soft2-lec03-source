#include <stdio.h>
#include <stdlib.h>

struct student {
    int id;
    struct student *next;
};

typedef struct student Student;

Student *push_front(Student *p, int id){
    Student *q = malloc(sizeof(Student));
    *q = (Student){.id = id, .next = p};
    return q;
}

int main(){
    Student *begin = NULL;
    begin = push_front(begin, 1);
    begin = push_front(begin, 2);
    begin = push_front(begin, 3);

    for (Student *p = begin; p != NULL; p = p->next){
        printf("%d\n", p->id);
    }
}
