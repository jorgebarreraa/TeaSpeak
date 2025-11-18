import { Modal } from "tc-shared/ui/elements/Modal";
export interface EventModalNewcomer {
    "show_step": {
        "step": "welcome" | "microphone" | "identity" | "finish";
    };
    "exit_guide": {
        ask_yesno: boolean;
    };
    "modal-shown": {};
    "action-next-help": {};
    "step-status": {
        allowNextStep: boolean;
        allowPreviousStep: boolean;
    };
}
export declare function openModalNewcomer(): Modal;
