import * as loader from "tc-loader";

import {
    DNSAddress,
    DNSProvider,
    DNSResolveOptions,
    DNSResolveResult,
} from "tc-shared/dns";
import * as dns_handler from "tc-native/dns";

export class NativeDnsResolver implements DNSProvider {
    resolveAddress(address: DNSAddress, options: DNSResolveOptions): Promise<DNSResolveResult> {
        return new Promise<DNSResolveResult>((resolve, reject) => {
            dns_handler.resolve_cr(address.hostname, address.port, result => {
                if(typeof result === "string") {
                    resolve({ status: "error", message: result });
                } else {
                    resolve({
                        status: "success",
                        originalAddress: address,
                        resolvedAddress: {
                            port: result.port,
                            hostname: result.host
                        }
                    });
                }
            });
        })
    }

    resolveAddressIPv4(address: DNSAddress, options: DNSResolveOptions): Promise<DNSResolveResult> {
        /* Currently only used to test if con-gate works which should not be required within the native client */
        return Promise.resolve({ status: "error", message: "not implemented" });
    }

}

loader.register_task(loader.Stage.JAVASCRIPT_INITIALIZING, {
    name: "Native DNS initialized",
    function: async () => {
        dns_handler.initialize();
    },
    priority: 100
});