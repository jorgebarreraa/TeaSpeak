pipeline {
    agent any

    parameters {
       booleanParam(defaultValue: true, description: 'Deploy this build', name: 'deploy_build')
       booleanParam(name: 'build_amd64', defaultValue: false, description: 'Build for amd64')
       booleanParam(name: 'build_arm32v7', defaultValue: false, description: 'Build for arm32v7')
       booleanParam(name: 'build_x86', defaultValue: false, description: 'Build for x86')
       choice(choices: ['none', 'debug', 'optimized', 'stable'], description: 'What kind of build do you want to run?', name: 'build_type')
       text(name: 'rebuild_libraries', defaultValue: '', description: 'Libraries to rebuild')
    }

    environment {
        //CMake specific flags
        CMAKE_OPTIONS = ''
        CMAKE_MAKE_OPTIONS = '-j32'

        //Make specific flags
        MAKE_OPTIONS = '-j32'

        force_rebuild="${params.rebuild_libraries}"
    }
    
    stages {
        /* build all amd64 stuff */
        stage ('build::amd64') {
            agent {
                label 'linux && amd64 && teaspeak'
            }

            when {
                expression { params.build_amd64 }
            }
                        
            environment {
                build_os_type="linux"
                build_os_arch="amd64"
            }
            
            stages {
                stage ('build::amd64::libraries') {
                    environment {
                        CMAKE_BUILD_TYPE="RelWithDebInfo" /* we build out libraries every time in release mode! (Performance improve) */
                    }

                    steps {
                        sh './attach_modules.sh'
                        sh 'cd libraries; ./build.sh'
                    }
                }
                
                
                stage ('build::amd64::build') {
                    steps {
                        sh "./build_teaspeak.sh ${params.build_type}"
                    }
                }

                stage ('build::amd64::deploy') {
                    when {
                        expression { params.deploy_build }
                    }

                    steps {
                        sh "cd TeaSpeak/server/repro/; chmod 400 build_private_key; ./build.sh linux/amd64_${params.build_type}"
                    }
                }
            }
        }

        /* build all x86 stuff */
        stage ('build::x86') {
            agent {
                label 'linux && x86 && teaspeak'
            }

            when {
                expression { params.build_x86 }
            }
                        
            environment {
                build_os_type="linux"
                build_os_arch="x86"
            }
            
            stages {
                stage ('build::x86::libraries') {
                    environment {
                        CMAKE_BUILD_TYPE="RelWithDebInfo" /* we build out libraries every time in release mode! (Performance improve) */
                    }

                    steps {
                        sh './attach_modules.sh'
                        sh 'cd libraries; ./build.sh'
                    }
                }
                
                
                stage ('build::x86::build') {
                    steps {
                        sh "./build_teaspeak.sh ${params.build_type}"
                    }
                }

                stage ('build::x86::deploy') {
                    when {
                        expression { params.deploy_build }
                    }

                    steps {
                        sh "cd TeaSpeak/server/repro/; chmod 400 build_private_key; ./build.sh linux/x86_${BUILD_TYPE}"
                    }
                }
            }
        }

        /* build all arm32v7 stuff */
        stage ('build::arm32v7') {
            agent {
                label 'linux && arm32v7 && teaspeak'
            }

            when {
                expression { params.build_arm32v7 }
            }
                        
            environment {
                build_os_type="linux"
                build_os_arch="arm32v7"
            }
            
            stages {
                stage ('build::arm32v7::libraries') {
                    environment {
                        CMAKE_BUILD_TYPE="RelWithDebInfo" /* we build out libraries every time in release mode! (Performance improve) */
                    }

                    steps {
                        sh './attach_modules.sh'
                        sh 'cd libraries; ./build.sh'
                    }
                }
                
                
                stage ('build::arm32v7::build') {
                    steps {
                        sh "./build_teaspeak.sh ${params.build_type}"
                    }
                }

                stage ('build::arm32v7::deploy') {
                    when {
                        expression { params.deploy_build }
                    }

                    steps {
                        sh "cd TeaSpeak/server/repro/; chmod 400 build_private_key; ./build.sh linux/arm32v7_${BUILD_TYPE}"
                    }
                }
            }
        }
    }
}
