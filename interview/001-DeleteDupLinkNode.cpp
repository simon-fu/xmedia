#include <stdio.h>

class ListNode {
public:
	int val;
	ListNode *next;
	ListNode(int x) : val(x), next(NULL) {}
};

static
ListNode* deleteDuplicates(ListNode* head){
    if(head != NULL){
        ListNode* p = head;
        ListNode* q = head;
        for( ; q != NULL; q = q->next){
            if(p->val != q->val){
                p->next = q;
                p = q;
            }
        }
        p->next = NULL; 
    }
    return head;
}

static
void printNodes(ListNode* head){
	while(head){
		printf("%d ", head->val);
		head = head->next;
	}
	printf("\n");
}

int main() {
	ListNode nodes[] = {
		ListNode(2),
		ListNode(2),
		ListNode(3),
		ListNode(5),
		ListNode(9),
		ListNode(9),
		ListNode(12),
		ListNode(15),
	};
	ListNode* p = &nodes[0];
	for(int i = 1; i < sizeof(nodes)/sizeof(nodes[0]); ++i){
		p->next = &nodes[i];
		p = p->next;
	}
	p->next = NULL;
	ListNode* head = &nodes[0];
	printNodes(head);
	head = deleteDuplicates(head);
	printNodes(head);
	return 0;
}


