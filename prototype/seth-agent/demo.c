/* Desktop demonstration only. No USB, CAN, Wi-Fi or ECU operations. */
#include "seth_agent.h"
#include <stdio.h>

static void display(const seth_agent_t *agent){
    seth_packet_t packet;
    seth_render(agent,&packet);
    printf("\nSETH / %s\n%s\n",packet.title?packet.title:"(untitled)",
        packet.text?packet.text:"(no text)");
    if(packet.detail)printf("%s\n",packet.detail);
    if(packet.question)printf("Question: %s\n",packet.question);
    for(unsigned i=0;i<packet.choice_count;i++)
        printf("  %u. %s\n",i+1,packet.choices[i]);
    printf("State: %s | ECU write access: NONE\n",packet.state?packet.state:"unknown");
}
int main(void){
    seth_agent_t agent;seth_init(&agent);
    display(&agent);
    seth_open_tour(&agent);seth_select_stage(&agent,6);
    display(&agent);
    seth_open_lesson(&agent,"fuel_trim_masking");
    display(&agent);
    seth_explain_alternate(&agent);
    display(&agent);
    seth_start_check(&agent);
    display(&agent);
    seth_open_build(&agent);
    seth_build_answer(&agent,"injection","pfi");
    seth_build_answer(&agent,"pump","pwm");
    display(&agent);
    seth_ask_diagnosis(&agent,"Idle wanders when warm");
    display(&agent);
    seth_open_report(&agent);
    display(&agent);
    return 0;
}
