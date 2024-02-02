//
// Created by just do it on 2024/2/2.
//
#include "forward/mqtt_events.h"
#include "forward/mqtt_rule_parser.h"
#include <stdio.h>

extern tmq_filter_expr_t* parse_filter_expression(const char* expr);
int main()
{
    tmq_rule_parser_init();
    tmq_filter_expr_t* filter = parse_filter_expression("((qos==1&&(username==zr||payload.x==abc))||client_id == 123)");
    if(!filter)
    {
        printf("syntax error\n");
        return 0;
    }
    tmq_print_filter_inorder(filter);
    printf("\n");
    tmq_print_filter_preorder(filter);
    printf("\n");
}