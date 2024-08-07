pipeline {
    agent any

    stages {
        stage('Clone build + test environment') {
            steps {
                sh 'rm -rf *' // clean up workspace
                git url: 'https://github.com/itsmevjnk/llz80emu-tests.git', branch: 'main'
                sh 'git submodule update --init --remote --recursive llz80emu json' // do not clone tests until building is successful
            }
        }
        
        stage('Build') {
            steps {
                sh '''
                    mkdir -p build && cd build
                    cmake ..
                    make
                    cd ..
                '''
            }
            post {
                success {
                    archiveArtifacts artifacts: 'build/llz80emu/libllz80emu*.*'
                }
            }
        }
        
        stage('Clone tests') {
            steps {
                sh 'git submodule update --init --remote --recursive tests'
            }
        }
        
        stage('Run tests') {
            steps {
                catchError(buildResult: 'UNSTABLE', stageResult: 'FAILURE') {
                    sh '''
                        for tc in tests/v1/*.json; do
                            ./build/tester/tester "$tc"
                        done
                    '''
                }
            }
            post {
                always {
                    emailext to: 'ngtv0404@gmail.com',
                        subject: "[Jenkins CI] ${currentBuild.fullDisplayName}: unit tests finished",
                        body: "${currentBuild.fullDisplayName} unit tests have finished.<br/>Last 30 lines of build log:<br/><pre>\${BUILD_LOG, maxLines=30, escapeHtml=true}</pre><br/>See the build's full log <a href='\${BUILD_URL}'>here</a>.",
                        mimeType: 'text/html'
                }
            }
        }

        stage('Code analysis (cppcheck) - bugs and code style') {
            steps {
                sh 'cppcheck --enable=all --inconclusive --xml --xml-version=2 llz80emu 2> cppcheck.xml'
            }
            
            post {
                always {
                    archiveArtifacts artifacts: 'cppcheck.xml'
                    echo "cppcheck report is available at ${env.BUILD_URL}artifacts/cppcheck.xml"
                }
            }
        }
        
        stage('Code analysis (Snyk) - security') {
            steps {
                catchError(buildResult: 'UNSTABLE', stageResult: 'FAILURE') {
                    snykSecurity(
                        snykInstallation: 'snyk',
                        snykTokenId: 'snyk-token',
                        additionalArguments: '--unmanaged'
                    )
                }
            }
            
            post {
                always {
                    emailext to: 'ngtv0404@gmail.com',
                        subject: "[Jenkins CI] ${currentBuild.fullDisplayName}: security analysis finished",
                        body: "${currentBuild.fullDisplayName} security analysis with Snyk has completed.<br/>Last 30 lines of build log:<br/><pre>\${BUILD_LOG, maxLines=30, escapeHtml=true}</pre><br/>See the full Snyk log <a href='\${BUILD_URL}/snykReport/'>here</a>. Also see the build's full log <a href='\${BUILD_URL}'>here</a>.",
                        mimeType: 'text/html'
                }
            }
        }
        
        stage('Deploy to staging') {
            steps {
                echo "building and testing results: ${BUILD_URL}" // no staging deployment for us as we do not want to pollute our GitHub repository
            }
        }
        
        stage('Integration tests on staging') {
            steps {
                timeout(time: 3, unit: 'DAYS') {
                    input message: 'Do you want to release this build to GitHub?', ok: 'Yes'
                }
            }
        }
        
        stage('Deploy to production') {
            steps {
                script {
                    COMMIT_SHA = sh(returnStdout: true, script: 'cd llz80emu && git rev-parse HEAD && cd ..')
                    echo "creating GitHub release for llz80emu ${COMMIT_SHA}"
                    createGitHubRelease(
                        credentialId: 'github-token',
                        repository: 'itsmevjnk/llz80emu',
                        tag: "jenkins-${BUILD_NUMBER}",
                        commitish: "${COMMIT_SHA}",
                        bodyText: "This release was automatically uploaded by Jenkins from [${currentBuild.fullDisplayName}](${BUILD_URL}), which was a **${currentBuild.currentResult}** build.",
                        prerelease: true
                    )
                    uploadGithubReleaseAsset(
                        credentialId: 'github-token',
                        repository: 'itsmevjnk/llz80emu',
                        tagName: "jenkins-${BUILD_NUMBER}",
                        uploadAssets: [
                            [filePath: "${WORKSPACE}/build/llz80emu/libllz80emu.so"],
                            [filePath: "${WORKSPACE}/build/llz80emu/libllz80emu_static.a"]
                        ]
                    )
                }
            }
        }
    }
}
